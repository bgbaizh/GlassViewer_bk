#include <iostream>
#include <iostream>
#include <exception>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <fstream>
#include <string>
#include <sstream>
#include <time.h>

using namespace std;

const int MAXNUMBEROFNEIGHBORS = 100;
const double PI = 3.141592653589793;
const int NILVALUE = 33333333;
const long int FACTORIALS[17] = {1,1,2,6,24,120,720,5040,40320,362880,3628800,39916800,479001600,6227020800,87178291200,1307674368000,20922789888000};


class Atom{
     
    public:
        Atom();
        virtual ~Atom();
        double posx,posy,posz;
        int neighbors[MAXNUMBEROFNEIGHBORS];
        double neighbordist[MAXNUMBEROFNEIGHBORS];
        double n_diffx[MAXNUMBEROFNEIGHBORS];
        double n_diffy[MAXNUMBEROFNEIGHBORS];
        double n_diffz[MAXNUMBEROFNEIGHBORS];
        double n_r[MAXNUMBEROFNEIGHBORS];
        double n_phi[MAXNUMBEROFNEIGHBORS];
        double n_theta[MAXNUMBEROFNEIGHBORS];
        int n_neighbors;

        double realQ4[9],imgQ4[9];
        double realQ6[13],imgQ6[13];
        double arealQ4[9],aimgQ4[9];
        double arealQ6[13],aimgQ6[13];
    
        double frenkelnumber;
        double avq6q6;

        int belongsto;
        int issolid;
        int structure;
        int id;
};


class System{
  
    public:
        void convert_to_spherical_coordinates(double , double , double , double &, double &, double &);
        double PLM(int, int, double);
        void YLM(int , int , double , double , double &, double &);
        void QLM(int ,int ,double ,double ,double &, double & );
        void get_all_neighbors();
        void calculate_complexQLM_6();
        double get_number_from_bond(int,int);
        void calculate_frenkel_numbers();
        double get_abs_distance(int,int,double&,double&,double&);
        System();
        virtual ~System();

        Atom* atoms;
    
        void read_particle_file();
        int calculate_nucsize();	//variant of function above
        int cluster_criteria(int,int );
        void find_solids();
        void find_clusters();
        int largest_cluster();
        void set_minfrenkel(int);
        void set_inputfile(string);
        void set_neighbordistance(double);
        void set_threshold(double);
        void set_avgthreshold(double);
    
        //old params
        int nop;
        int baseunit;
        int minfrenkel;
        double boxx, boxy, boxz;
        string inputfile;
        double neighbordistance;
        double threshold;
        double avgthreshold;

};
