/*
  Mini serveur WebSocket (RFC 6455) pour l'ESP32 — aucune bibliothèque à installer.

  Pourquoi : avec HTTP, l'IHM devait ouvrir une nouvelle connexion à chaque échange
  (plusieurs dizaines de ms à chaque fois). Avec un WebSocket, la connexion reste
  ouverte : le robot POUSSE la télémétrie 20 fois par seconde et reçoit les
  commandes de l'IHM instantanément.

  Limites volontaires (suffisantes pour l'IHM) : messages texte, < 1 Ko reçus,
  pas de fragmentation, 4 navigateurs connectés au maximum.

  SÉCURITÉ : les envois sont NON BLOQUANTS. Si un navigateur disparaît sans fermer la
  connexion (téléphone qui quitte le WiFi, PC en veille), un envoi classique pourrait
  bloquer la boucle du robot plusieurs secondes — et donc le chien de garde des
  moteurs. Ici, une trame qui ne peut pas partir est simplement sautée, et un client
  bloqué ~2 s est déconnecté.
*/
#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <errno.h>
#if defined(ESP32)
#include <lwip/sockets.h>
#else
#include <sys/socket.h>
#endif

class ServeurWebSocket {
 public:
  typedef void (*RappelMessage)(uint8_t client, char* message, size_t longueur);
  typedef void (*RappelConnexion)(uint8_t client, bool connecte);

  static const uint8_t MAX_CLIENTS = 4;
  static const size_t TAILLE_TAMPON = 1024;
  static const unsigned long TIMEOUT_SILENCE_MS = 5000; // l'IHM envoie au moins 1 message/s
  static const uint8_t MAX_TRAMES_SAUTEES = 40;          // ~2 s de télémétrie impossible à envoyer

  explicit ServeurWebSocket(uint16_t port) : serveur(port) {}

  void begin() {
    serveur.begin();
    serveur.setNoDelay(true);
  }
  void surMessage(RappelMessage r) { rappelMessage = r; }
  void surConnexion(RappelConnexion r) { rappelConnexion = r; }

  uint8_t nbClients() const {
    uint8_t n = 0;
    for (uint8_t i = 0; i < MAX_CLIENTS; i++) if (clients[i].etat == OUVERT) n++;
    return n;
  }

  // À appeler à chaque tour de loop() : accepte, lit et décode sans jamais bloquer.
  void loop() {
    accepterNouveaux();
    for (uint8_t i = 0; i < MAX_CLIENTS; i++) traiterClient(i);
  }

  void envoyer(uint8_t i, const char* texte, size_t n) {
    if (i < MAX_CLIENTS && clients[i].etat == OUVERT) envoyerTrame(i, 0x1, (const uint8_t*)texte, n);
  }
  void diffuser(const char* texte, size_t n) {
    for (uint8_t i = 0; i < MAX_CLIENTS; i++) envoyer(i, texte, n);
  }

  // Outils publics pour pouvoir les tester séparément
  static void sha1(const uint8_t* msg, size_t len, uint8_t sortie[20]) {
    uint32_t h[5] = { 0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0 };
    size_t nbBlocs = (len + 9 + 63) / 64;
    for (size_t b = 0; b < nbBlocs; b++) {
      uint8_t bloc[64];
      for (size_t k = 0; k < 64; k++) {
        size_t pos = b * 64 + k;
        bloc[k] = pos < len ? msg[pos] : (pos == len ? 0x80 : 0);
      }
      if (b == nbBlocs - 1) {
        uint64_t bits = (uint64_t)len * 8;
        for (int k = 0; k < 8; k++) bloc[63 - k] = (uint8_t)(bits >> (8 * k));
      }
      uint32_t w[80];
      for (int t = 0; t < 16; t++) {
        w[t] = ((uint32_t)bloc[4 * t] << 24) | ((uint32_t)bloc[4 * t + 1] << 16) | ((uint32_t)bloc[4 * t + 2] << 8) | bloc[4 * t + 3];
      }
      for (int t = 16; t < 80; t++) w[t] = rotation(w[t - 3] ^ w[t - 8] ^ w[t - 14] ^ w[t - 16], 1);
      uint32_t a = h[0], bb = h[1], c = h[2], d = h[3], e = h[4];
      for (int t = 0; t < 80; t++) {
        uint32_t f, k;
        if (t < 20)      { f = (bb & c) | (~bb & d);           k = 0x5A827999; }
        else if (t < 40) { f = bb ^ c ^ d;                     k = 0x6ED9EBA1; }
        else if (t < 60) { f = (bb & c) | (bb & d) | (c & d);  k = 0x8F1BBCDC; }
        else             { f = bb ^ c ^ d;                     k = 0xCA62C1D6; }
        uint32_t temp = rotation(a, 5) + f + e + k + w[t];
        e = d; d = c; c = rotation(bb, 30); bb = a; a = temp;
      }
      h[0] += a; h[1] += bb; h[2] += c; h[3] += d; h[4] += e;
    }
    for (int i = 0; i < 5; i++) {
      sortie[4 * i] = h[i] >> 24; sortie[4 * i + 1] = h[i] >> 16; sortie[4 * i + 2] = h[i] >> 8; sortie[4 * i + 3] = h[i];
    }
  }

  static void base64(const uint8_t* in, size_t len, char* sortie) {
    static const char* alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t o = 0;
    for (size_t i = 0; i < len; i += 3) {
      uint32_t v = (uint32_t)in[i] << 16;
      if (i + 1 < len) v |= (uint32_t)in[i + 1] << 8;
      if (i + 2 < len) v |= in[i + 2];
      sortie[o++] = alphabet[(v >> 18) & 63];
      sortie[o++] = alphabet[(v >> 12) & 63];
      sortie[o++] = i + 1 < len ? alphabet[(v >> 6) & 63] : '=';
      sortie[o++] = i + 2 < len ? alphabet[v & 63] : '=';
    }
    sortie[o] = 0;
  }

  // Sec-WebSocket-Accept = base64(sha1(clé + GUID)) — sortie : 29 octets minimum
  static void calculerAccept(const char* cle, char* sortie) {
    char concat[128];
    snprintf(concat, sizeof(concat), "%s258EAFA5-E914-47DA-95CA-C5AB0DC85B11", cle);
    uint8_t empreinte[20];
    sha1((const uint8_t*)concat, strlen(concat), empreinte);
    base64(empreinte, 20, sortie);
  }

 private:
  enum Etat : uint8_t { LIBRE, POIGNEE_DE_MAIN, OUVERT };
  struct Client {
    WiFiClient tcp;
    Etat etat = LIBRE;
    uint8_t tampon[TAILLE_TAMPON];
    size_t n = 0;
    unsigned long dernierMs = 0;
    uint8_t tramesSautees = 0;
  };

  WiFiServer serveur;
  Client clients[MAX_CLIENTS];
  RappelMessage rappelMessage = nullptr;
  RappelConnexion rappelConnexion = nullptr;

  static uint32_t rotation(uint32_t x, int n) { return (x << n) | (x >> (32 - n)); }

  static bool commencePar(const char* texte, const char* prefixe) { // insensible à la casse
    for (; *prefixe; texte++, prefixe++) {
      if (tolower((unsigned char)*texte) != tolower((unsigned char)*prefixe)) return false;
    }
    return true;
  }

  void accepterNouveaux() {
    WiFiClient nouveau = serveur.accept();
    if (!nouveau) return;
    for (uint8_t i = 0; i < MAX_CLIENTS; i++) {
      if (clients[i].etat == LIBRE) {
        clients[i].tcp = nouveau;
        clients[i].tcp.setNoDelay(true);
        clients[i].etat = POIGNEE_DE_MAIN;
        clients[i].n = 0;
        clients[i].dernierMs = millis();
        clients[i].tramesSautees = 0;
        return;
      }
    }
    nouveau.stop(); // plus de place
  }

  void fermer(uint8_t i) {
    bool etaitOuvert = clients[i].etat == OUVERT;
    clients[i].tcp.stop();
    clients[i].etat = LIBRE;
    clients[i].n = 0;
    if (etaitOuvert && rappelConnexion) rappelConnexion(i, false);
  }

  void traiterClient(uint8_t i) {
    Client& c = clients[i];
    if (c.etat == LIBRE) return;
    if (!c.tcp.connected()) { fermer(i); return; }

    int dispo = c.tcp.available();
    if (dispo > 0) {
      size_t place = TAILLE_TAMPON - c.n;
      if (place == 0) { fermer(i); return; }
      size_t aLire = (size_t)dispo < place ? (size_t)dispo : place;
      int lu = c.tcp.read(c.tampon + c.n, aLire);
      if (lu > 0) { c.n += lu; c.dernierMs = millis(); }
    }

    if (c.etat == POIGNEE_DE_MAIN) traiterPoigneeDeMain(i);
    else traiterTrames(i);

    if (c.etat != LIBRE && millis() - c.dernierMs > TIMEOUT_SILENCE_MS) fermer(i);
  }

  void traiterPoigneeDeMain(uint8_t i) {
    Client& c = clients[i];
    size_t fin = 0;
    for (size_t k = 3; k < c.n; k++) {
      if (c.tampon[k - 3] == '\r' && c.tampon[k - 2] == '\n' && c.tampon[k - 1] == '\r' && c.tampon[k] == '\n') { fin = k + 1; break; }
    }
    if (fin == 0) {
      if (c.n >= TAILLE_TAMPON) fermer(i); // en-têtes trop longs
      return;
    }

    // Recherche de l'en-tête "Sec-WebSocket-Key:"
    char cle[64] = "";
    const char* p = (const char*)c.tampon;
    const char* finEntetes = p + fin;
    while (p < finEntetes) {
      if (commencePar(p, "Sec-WebSocket-Key:")) {
        p += strlen("Sec-WebSocket-Key:");
        while (*p == ' ') p++;
        size_t l = 0;
        while (p < finEntetes && *p != '\r' && l < sizeof(cle) - 1) cle[l++] = *p++;
        cle[l] = 0;
        break;
      }
      while (p < finEntetes && *p != '\n') p++;
      p++;
    }

    if (cle[0] == 0) {
      const char* refus = "HTTP/1.1 400 Bad Request\r\nConnection: close\r\n\r\nWebSocket attendu\r\n";
      ecrire(i, (const uint8_t*)refus, strlen(refus));
      fermer(i);
      return;
    }

    char accept[32];
    calculerAccept(cle, accept);
    char reponse[200];
    int l = snprintf(reponse, sizeof(reponse),
                     "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: %s\r\n\r\n", accept);
    if (!ecrire(i, (const uint8_t*)reponse, l)) { fermer(i); return; }

    memmove(c.tampon, c.tampon + fin, c.n - fin);
    c.n -= fin;
    c.etat = OUVERT;
    c.dernierMs = millis();
    if (rappelConnexion) rappelConnexion(i, true);
  }

  void traiterTrames(uint8_t i) {
    Client& c = clients[i];
    while (c.etat == OUVERT && c.n >= 2) {
      bool finale = c.tampon[0] & 0x80;
      uint8_t opcode = c.tampon[0] & 0x0F;
      bool masquee = c.tampon[1] & 0x80;
      size_t longueur = c.tampon[1] & 0x7F;
      size_t entete = 2;
      if (longueur == 126) {
        if (c.n < 4) return;
        longueur = ((size_t)c.tampon[2] << 8) | c.tampon[3];
        entete = 4;
      } else if (longueur == 127) {
        fermer(i); return; // messages géants refusés
      }
      if (!masquee || !finale) { fermer(i); return; } // un navigateur masque toujours ; pas de fragmentation
      entete += 4;
      if (entete + longueur >= TAILLE_TAMPON) { fermer(i); return; }
      if (c.n < entete + longueur) return; // trame incomplète : on attend la suite

      uint8_t* masque = c.tampon + entete - 4;
      uint8_t* charge = c.tampon + entete;
      for (size_t k = 0; k < longueur; k++) charge[k] ^= masque[k & 3];

      if (opcode == 0x1) {            // texte
        uint8_t sauve = charge[longueur];
        charge[longueur] = 0;
        if (rappelMessage) rappelMessage(i, (char*)charge, longueur);
        charge[longueur] = sauve;
      } else if (opcode == 0x8) {     // fermeture
        envoyerTrame(i, 0x8, nullptr, 0);
        fermer(i);
        return;
      } else if (opcode == 0x9) {     // ping -> pong
        envoyerTrame(i, 0xA, charge, longueur);
      }                               // pong / binaire : ignorés
      if (c.etat != OUVERT) return;   // le client a pu être fermé pendant un envoi

      size_t total = entete + longueur;
      memmove(c.tampon, c.tampon + total, c.n - total);
      c.n -= total;
    }
  }

  // Envoi non bloquant : tout ou rien (une trame coupée en deux rendrait le flux invalide).
  bool ecrire(uint8_t i, const uint8_t* donnees, size_t n) {
    int fd = clients[i].tcp.fd();
    if (fd < 0) { fermer(i); return false; }
    int r = send(fd, donnees, n, MSG_DONTWAIT);
    if (r == (int)n) { clients[i].tramesSautees = 0; return true; }
    if (r > 0 || (r < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) { fermer(i); return false; }
    if (++clients[i].tramesSautees > MAX_TRAMES_SAUTEES) fermer(i); // le navigateur ne lit plus
    return false;
  }

  void envoyerTrame(uint8_t i, uint8_t opcode, const uint8_t* donnees, size_t n) {
    static uint8_t trame[2048];
    size_t e = n < 126 ? 2 : 4;
    if (e + n > sizeof(trame)) return; // trop gros pour ce mini serveur (la télémétrie fait ~1 Ko)
    trame[0] = 0x80 | opcode;
    if (n < 126) { trame[1] = (uint8_t)n; }
    else { trame[1] = 126; trame[2] = (uint8_t)(n >> 8); trame[3] = (uint8_t)(n & 0xFF); }
    if (n) memcpy(trame + e, donnees, n);
    ecrire(i, trame, e + n); // un seul envoi = un seul paquet TCP
  }
};
