#include <iostream>
#include <string>

using namespace std;

class Voiture
{
private:
    string couleur;
    double poids;
    int prix;

public:
    // Constructeur par défaut : n'initialise rien
    Voiture() {}

    // Constructeur paramétré
    Voiture(string couleur, double poids, int prix)
        : couleur(couleur), poids(poids), prix(prix) {}

    // Destructeur
    ~Voiture()
    {
        cout << "je suis détruite" << endl;
    }

    // Getters
    string getCouleur() const { return couleur; }
    double getPoids() const { return poids; }
    int getPrix() const { return prix; }

    // Setters
    void setCouleur(string c) { couleur = c; }
    void setPoids(double p) { poids = p; }
    void setPrix(int p) { prix = p; }
};

int main()
{
    // Voiture créée avec le constructeur paramétré
    Voiture v1("rouge", 1250.5, 18000);
    cout << "v1 : " << v1.getCouleur() << ", " << v1.getPoids()
         << " kg, " << v1.getPrix() << " euros" << endl;

    // Voiture créée avec le constructeur par défaut, puis remplie avec les setters
    Voiture v2;
    v2.setCouleur("bleue");
    v2.setPoids(980.0);
    v2.setPrix(12500);
    cout << "v2 : " << v2.getCouleur() << ", " << v2.getPoids()
         << " kg, " << v2.getPrix() << " euros" << endl;

    // Modification d'un attribut
    v1.setPrix(16500);
    cout << "Nouveau prix de v1 : " << v1.getPrix() << " euros" << endl;

    // Voiture allouée dynamiquement : le destructeur est appelé par delete
    Voiture* v3 = new Voiture("noire", 1400.0, 25000);
    cout << "v3 : " << v3->getCouleur() << endl;
    delete v3;

    cout << "Fin du main" << endl;
    return 0; // v2 puis v1 sont détruites ici
}
