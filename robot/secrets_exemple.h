/*
  Réseaux WiFi externes OPTIONNELS du robot.

  1. Copier ce fichier sous le nom "secrets.h" (dans ce même dossier robot/).
  2. Remplacer les mots de passe.
  secrets.h est ignoré par git : tes mots de passe ne partent pas sur GitHub.

  Le robot crée TOUJOURS son point d'accès ROBOT_ESP32 : ces réseaux ne servent
  qu'à un accès en plus (dashboard depuis la box, Internet pour le portail WIFI-UP...).
  Le premier réseau de la liste qui est à portée est utilisé.
*/
ReseauWifi reseaux[] = {
  { "Wi-Fi personnel",       "Livebox-9A6C",     "MOT_DE_PASSE_ICI" },
  { "Partage de connexion",  "laolive_originel", "MOT_DE_PASSE_ICI" },
  { "WIFI-UP (IUT, ouvert)", "WIFI-UP",          "" }, // portail captif : mot de passe WiFi VIDE ("")
};
