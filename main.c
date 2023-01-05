#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"

// Enum pour représenter les différentes classes de véhicules
enum classes_vehicule {
    LEGER,
    INTERMEDIAIRE,
    LOURD_2_ESSIEUX,
    LOURD_3_ESSIEUX,
    MOTO
};

// Tableau pour représenter les différentes classes de véhicules avec leur nom
const char *classes_vehicule_str[] = {
        "Leger",
        "Intermediaire",
        "Lourd 2 essieux",
        "Lourd 3 essieux",
        "Moto"
};

// Enum pour représenter les différents modes de paiement
enum mode_paiement {
    ESPECES,
    TELEPEAGE
};

// Tableau pour représenter les différents modes de paiement avec leur nom
const char *mode_paiement_str[] = {
        "Especes",
        "Telepeage"
};

// Structure pour représenter un véhicule
struct vehicule {
    int id;                             // Identifiant du véhicule
    enum classes_vehicule classe;       // Classe du véhicule
    enum mode_paiement mode_paiement;   // Mode de paiement
    int nb_occupant;                    // Nombre d'occupants du véhicule, on se limitera à 1 ou 2 occupants pour la simulation
};

// Structure pour représenter un poste de péage
struct poste_peage {
    int id;                         // Identifiant du poste de péage
    int total_collectee;            // Total de taxes collectées
    pthread_mutex_t mutex;          // Mutex pour protéger les variables partagées
    sem_t sem;                      // Semaphore pour contrôler l'accès aux variables partagées
    struct vehicule *queue;         // File d'attente de véhicules
    int taille_queue;               // Taille de la file d'attente
    int tete_queue;                 // Tête de la file d'attente
    int fin_queue;                  // Queue de la file d'attente
};


// Tableau de structures pour représenter les postes de péage
struct poste_peage *postes_peage;

// Tableau de taux de paiement par véhicule
int montants_classe[5] = {20, 30, 40, 50, 10};

// Temps d'attente en secondes pour chaque classe de véhicule (voitures, motos, deux-essieux, trois-essieux)
const int TEMPS_ATTENTE[5] = {13, 15, 20, 25, 12};

// Variables globales pour tout le programme.
// Elles sont initialisées modifiées en fonction des arguments passés au programme
int nb_vehicules = 100;             // Nombre de véhicules créés
int nb_postes_peage = 5;            // Nombre de postes de péage
int voie_telepeage = 0;             // S'il y a une voie de télépéage (1) ou non (0)
int voie_covoiturage = 0;           // S'il y a une voie de covoiturage (1) ou non (0)
int temps_entre_vehicules = 1000;   // Temps entre la création de deux véhicules en millisecondes


// Variables globales pour la simulation
int nb_telepeage = 0;           // Nombre de véhicules ayant utilisé la voie de télépéage
int nb_sans_telepeage = 0;      // Nombre de véhicules n'ayant pas utilisé la voie de télépéage
int nb_covoiturage = 0;         // Nombre de véhicules ayant utilisé la voie de covoiturage
int nb_sans_covoiturage = 0;    // Nombre de véhicules n'ayant pas utilisé la voie de covoiturage


// Fonction pour initialiser un poste de péage
void init_poste_peage(int id) {
    postes_peage[id].id = id;
    postes_peage[id].total_collectee = 0;
    postes_peage[id].queue = malloc((nb_vehicules+1) * sizeof(struct vehicule));
    pthread_mutex_init(&postes_peage[id].mutex, NULL);
    sem_init(&postes_peage[id].sem, 0, 1);
    postes_peage[id].taille_queue = 100;
    postes_peage[id].tete_queue = 0;
    postes_peage[id].fin_queue = 0;
}

// Fonction pour créer un véhicule
struct vehicule creation_vehicule(int id) {
    struct vehicule v;
    v.id = id;
    v.classe = rand() % 5;

    // On choisit un mode de paiement aléatoire pondéré (7/10 en espèces, 3/10 en télépéage)
    int mode_paiement = rand() % 10;
    switch (mode_paiement) {
        case 7:
        case 8:
        case 9:
            mode_paiement = TELEPEAGE;
            break;
        case 0:
        case 1:
        case 2:
        case 3:     // Tous les choix ont été laissés pour plus de lisibilité
        case 4:
        case 5:
        case 6:
            mode_paiement = ESPECES;
            break;
        default:
            mode_paiement = ESPECES;
            break;
    }
    v.mode_paiement = mode_paiement;

    // Si le véhicule est une moto, il n'y a qu'un seul occupant et donc pas de covoiturage
    if (v.classe == MOTO) {
        v.nb_occupant = 1;
    } else {
        v.nb_occupant = rand() % 2 + 1;
    }
    return v;
}

// Fonction pour ajouter un véhicule à la file d'attente d'un poste de péage
void ajout_queue(struct poste_peage* tb, struct vehicule v) {
    // Attendre que le semaphore soit disponible
    sem_wait(&tb->sem);

    // Ajouter le véhicule à la file d'attente
    tb->queue[tb->fin_queue] = v;
    tb->fin_queue = (tb->fin_queue + 1) % tb->taille_queue;

    // Relâcher le semaphore
    sem_post(&tb->sem);
}

// Fonction pour retirer un véhicule de la file d'attente d'un poste de péage
struct vehicule supp_queue(struct poste_peage* tb) {
    struct vehicule v;

    // Attendre que le semaphore soit disponible
    sem_wait(&tb->sem);

    // Retirer le véhicule de la file d'attente
    v = tb->queue[tb->tete_queue];
    tb->tete_queue = (tb->tete_queue + 1) % tb->taille_queue;

    // Relâcher le semaphore
    sem_post(&tb->sem);

    return v;
}

// Fonction pour obtenir la plus petite file d'attente d'un poste de péage
int poste_peage_minimum(int correction) {
    int poste_peage_id = 0;
    int nb = postes_peage[0].fin_queue - postes_peage[0].tete_queue;
    for (int i = 1; i < (nb_postes_peage - correction); i++) {
        if ((postes_peage[i].fin_queue - postes_peage[i].tete_queue) < nb) {
            nb = (postes_peage[i].fin_queue - postes_peage[i].tete_queue);
            poste_peage_id = i;
        }
    }
    return poste_peage_id;
}

// Fonction pour afficher l'entrée ou la sortie d'un véhicule
void affichage(struct vehicule v, int poste_peage_id, int sortie) {
    if (sortie == 0) {
        printf(ANSI_COLOR_GREEN"V%chicule entrant %d"ANSI_COLOR_RESET": classe %s, ",
               130, v.id, classes_vehicule_str[v.classe]);
        if (v.mode_paiement == TELEPEAGE) {
            printf(ANSI_COLOR_YELLOW"mode de paiement %s"ANSI_COLOR_RESET, mode_paiement_str[v.mode_paiement]);
        } else {
            printf(ANSI_COLOR_BLUE"mode de paiement %s"ANSI_COLOR_RESET, mode_paiement_str[v.mode_paiement]);
        }
        printf(", poste de p%cage %d, nombre d'occupants %d\n", 130, poste_peage_id, v.nb_occupant);
    } else {
        printf(ANSI_COLOR_RED"V%chicule sortant %d"ANSI_COLOR_RESET": classe %s, poste de p%cage %d\n",
               130, v.id, classes_vehicule_str[v.classe], 130, poste_peage_id);
    }
}

// Fonction pour simuler le passage d'un véhicule au péage
void* simulate_vehicule(void* arg) {
    // Récupérer les informations du véhicule
    struct vehicule* v = (struct vehicule*)arg;

    // Déterminer le poste de péage à utiliser en fonction du mode de paiement
    int poste_peage_id;
    if (voie_telepeage == 1) {
        if (v->mode_paiement == TELEPEAGE) {
            poste_peage_id = nb_postes_peage - 1;
        } else {
            if (voie_covoiturage == 1) {
                if (v->nb_occupant > 1) {
                    poste_peage_id = nb_postes_peage - 2;
                    nb_covoiturage++;
                } else {
                    poste_peage_id = poste_peage_minimum(2);
                    nb_sans_covoiturage++;
                }
            } else {
                poste_peage_id = poste_peage_minimum(1);
            }
        }
    } else {
        poste_peage_id = poste_peage_minimum(1);
    }

    // Ajouter le véhicule à la file d'attente du poste de péage
    affichage(*v, poste_peage_id, 0);
    ajout_queue(&postes_peage[poste_peage_id], *v);

    // Attendre au poste de péage
    // l'attente dépend de la classe du véhicule et de la voie de télépéage (s'il y en a une)
    sleep(1);
    if (voie_telepeage == 1) {
        if (v->mode_paiement == TELEPEAGE) {
            sleep(1);
            nb_telepeage++;
        } else {
            if (voie_covoiturage == 1) {
                if (v->nb_occupant > 1) {
                    sleep(2);
                    nb_covoiturage++;
                } else {
                    sleep(TEMPS_ATTENTE[v->classe]);
                    nb_sans_covoiturage++;
                }
            } else {
                sleep(TEMPS_ATTENTE[v->classe]);
                nb_sans_covoiturage++;
            }
            nb_sans_telepeage++;
        }
    } else {
        sleep(TEMPS_ATTENTE[v->classe]);
        nb_sans_telepeage++;
    }

    // Passer au péage
    struct vehicule vehicule_passe = supp_queue(&postes_peage[poste_peage_id]);
    affichage(vehicule_passe, poste_peage_id, 1);

    // Calcule du montant à payer
    int total_paye = montants_classe[vehicule_passe.classe];

    // Prendre le verrou sur le mutex pour mettre à jour la variable partagée
    pthread_mutex_lock(&postes_peage[poste_peage_id].mutex);

    // Mettre à jour le total de taxes collectées
    postes_peage[poste_peage_id].total_collectee += total_paye;

    // Relâcher le verrou sur le mutex
    pthread_mutex_unlock(&postes_peage[poste_peage_id].mutex);

    return NULL;
}

int init_argument(int argc, char* argv[]) {
    // Initialise les variables globales
    switch (argc) {
        case 1:
            printf("Possibilit%c d'utilisation: ./%s "
                   "<nb_vehicules> "
                   "<nb_postes_peage> "
                   "<voie_telepeage> "
                   "<voie_covoiturage> "
                   "<temps_entre_vehicule>\n", 130, argv[0]);
            printf("Valeur par d%cfaut\n", 130);
            break;
        case 2:
            nb_vehicules = atoi(argv[1]);
            break;
        case 3:
            nb_vehicules = atoi(argv[1]);
            nb_postes_peage = atoi(argv[2]);
            break;
        case 4:
            nb_vehicules = atoi(argv[1]);
            nb_postes_peage = atoi(argv[2]);
            voie_telepeage = atoi(argv[3]);
            if (voie_telepeage != 0 && voie_telepeage != 1) {
                printf("Erreur: presence d'une voie de t%cl%cp%cage 0 (Non) ou 1 (oui)\n", 130, 130, 130);
                return 1;
            }
            break;
        case 5:
            nb_vehicules = atoi(argv[1]);
            nb_postes_peage = atoi(argv[2]);
            voie_telepeage = atoi(argv[3]);
            if (voie_telepeage != 0 && voie_telepeage != 1) {
                printf("Erreur: presence d'une voie de t%cl%cp%cage 0 (Non) ou 1 (oui)\n", 130, 130, 130);
                return 1;
            }
            voie_covoiturage = atoi(argv[4]);
            if (voie_covoiturage != 0 && voie_covoiturage != 1) {
                printf("Erreur: presence d'une voie de covoiturage 0 (Non) ou 1 (oui)\n");
                return 1;
            }
            break;
        case 6:
            nb_vehicules = atoi(argv[1]);
            nb_postes_peage = atoi(argv[2]);
            voie_telepeage = atoi(argv[3]);
            if (voie_telepeage != 0 && voie_telepeage != 1) {
                printf("Erreur: presence d'une voie de t%cl%cp%cage 0 (Non) ou 1 (oui)\n", 130, 130, 130);
                return 1;
            }
            voie_covoiturage = atoi(argv[4]);
            if (voie_covoiturage != 0 && voie_covoiturage != 1) {
                printf("Erreur: presence d'une voie de covoiturage 0 (Non) ou 1 (oui)\n");
                return 1;
            }
            temps_entre_vehicules = atoi(argv[5]);
            if (temps_entre_vehicules <= 0) {
                printf("Erreur: le temps entre les v%chicules doit %ctre positif et non nul\n", 130, 136);
                return 1;
            }
            if (temps_entre_vehicules < 120) {
                printf("Attention: le temps entre les v%chicules est tres court, "
                       "les r%csultats seront erron%cs\n", 130, 130, 130);
            }
            if (temps_entre_vehicules > 2000) {
                printf("Attention: le temps entre les v%chicules est tres long\n", 130);
            }
            break;
        default:
            printf("Erreur: trop d'arguments, valeurs par d%cfaut\n", 130);
            break;
    }
    return 0;
}


int main(int argc, char* argv[]) {

    // Initialisation des variables globales
    if (init_argument(argc, argv) != 0) {
        return 1;
    }

    // Affiche les paramètres
    printf("Param%ctres:\n"
           "\t- Nombre de v%chicules = %d\n"
           "\t- Nombre de postes de p%cage = %d\n"
           "\t- Voie de t%cl%cp%cage = %d\n"
           "\t- Voie de covoiturage = %d\n"
           "\t- Temps entre les v%chicules (en second) = %.3f\n",
           136,
           130, nb_vehicules,
           130, nb_postes_peage,
           130, 130, 130, voie_telepeage,
           voie_covoiturage,
           130, temps_entre_vehicules/1000.0);
    printf("\n\n ========================== Debut du programme (dans 2 sec) ========================== \n\n");
    sleep(2);

    clock_t start = clock();

    // Initialiser les postes de péage
    postes_peage = malloc(nb_postes_peage * sizeof(struct poste_peage));
    for (int i = 0; i < nb_postes_peage; i++) {
        init_poste_peage(i);
    }

    // Créer les threads pour simuler les véhicules
    pthread_t threads[nb_vehicules];
    for (int i = 0; i < nb_vehicules; i++) {
        struct vehicule v = creation_vehicule(i);
        pthread_create(&threads[i], NULL, simulate_vehicule, &v);
        usleep(temps_entre_vehicules * 1000);
    }

    // Attendre la fin des threads
    for (int i = 0; i < nb_vehicules; i++) {
        pthread_join(threads[i], NULL);
    }

    clock_t end = clock();

    double time_spent = (double)(end - start) / CLOCKS_PER_SEC;

    printf("\n\n ========================== Fin du programme ========================== \n\n");
    // Afficher le total de taxes collectées par chaque poste de péage
    for (int i = 0; i < nb_postes_peage; i++) {
        printf("Total collecte au poste de p%cage %d: %d\n", 130, postes_peage[i].id, postes_peage[i].total_collectee);
    }
    printf("\n");

    // Afficher le nombre de véhicules qui ont utilisé le télépéage
    // et ceux qui n'ont pas utilisé le télépéage si la voie de télépéage est définie
    if (voie_telepeage == 1) {
        printf("Nombre de v%chicules ayant utilise la voie de t%cl%cp%cage: %d\n", 130, 130, 130, 130, nb_telepeage);
        printf("Nombre de v%chicules n'ayant pas utilise la voie de t%cl%cp%cage: %d\n", 130, 130, 130, 130, nb_sans_telepeage);
    }

    // Afficher le nombre de véhicules qui ont utilisé le covoiturage
    // et ceux qui n'ont pas utilisé le covoiturage si la voie de covoiturage est définie
    if (voie_covoiturage == 1) {
        printf("Nombre de v%chicules ayant utilise la voie de covoiturage: %d\n", 130, nb_covoiturage);
        printf("Nombre de v%chicules n'ayant pas utilise la voie de covoiturage: %d\n", 130, nb_sans_covoiturage);
    }

    printf("Temps d'ex%ccution: %.2f secondes\n", 130, time_spent);

    // Libérer la mémoire
    for (int i = 0; i < nb_postes_peage; ++i) {
        free(postes_peage[i].queue);
    }
    free(postes_peage);

    return 0;
}
