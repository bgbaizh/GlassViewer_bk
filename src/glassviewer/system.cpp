#include "system.h"
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <stdio.h>
#include "voro++.hh"
#include "string.h"
#include <chrono>
#include <pybind11/stl.h>
#include <thread>
#include <mutex>

using namespace voro;

//-----------------------------------------------------
// Constructor, Destructor and Access functions
//-----------------------------------------------------
System::System(){

    nop = 0;
    ghost_nop = 0;
    real_nop = 0;
    triclinic = 0;
    usecells = 0;
    filter = 0;
    maxclusterid = -1;
    
    alpha = 1;
    voronoiused = 0;
    solidq = 6;
    criteria = 0;
    comparecriteria = 0;
    
    neighbordistance = 0;
    pdf_halftimes=0;
    //set box with zeros
    for(int i=0; i<3; i++){
        for(int j=0; j<3; j++){
            box[i][j] = 0.0;
        }
    }

}

System::~System(){
}

//-----------------------------------------------------
// Simulation box related methods
//-----------------------------------------------------
void System::assign_triclinic_params(vector<vector<double>> drot, vector<vector<double>> drotinv){

    for(int i=0; i<3; i++){
        for(int j=0; j<3; j++){
            rot[i][j] = drot[i][j];
            rotinv[i][j] = drotinv[i][j];
        }
    }

    triclinic = 1;
}

vector<vector<double>> System::get_triclinic_params(){

    vector<vector<double>> drot;
    vector<double> dummydrot;
    for(int i=0; i<3; i++){
        dummydrot.clear();
        for(int j=0; j<3; j++){
            dummydrot.emplace_back(rot[i][j]);
        }
        drot.emplace_back(dummydrot);
    }
    return drot;
}

void System::sbox(vector<vector <double>> boxd) {

    //this method will be redone to get a 3x3 box
    //always. They will be then translated to the
    //corresponding other boxes
    double isum;

    for(int i=0; i<3; i++){
        isum = 0;
        for(int j=0; j<3; j++){
            box[i][j] = boxd[i][j];
            isum += boxd[i][j]*boxd[i][j];
        }
        boxdims[i][0] = 0;
        boxdims[i][1] = sqrt(isum);

    }

    boxx = boxdims[0][1] - boxdims[0][0];
    boxy = boxdims[1][1] - boxdims[1][0];
    boxz = boxdims[2][1] - boxdims[2][0];
}

vector<vector<double>> System::gbox(){
    vector<vector<double>> qres;
    vector<double> qd;

    for(int i=0;i<3;i++){
        qd.clear();
        for(int j=0;j<3;j++){
            qd.emplace_back(box[i][j]);
        }
        qres.emplace_back(qd);
    }
    return qres;
}


//-----------------------------------------------------
// Atom related methods
//-----------------------------------------------------
//this function allows for handling custom formats of atoms and so on
void System::set_atoms( vector<Atom> atomitos){

    atoms.clear();
    nop = atomitos.size();
    atoms.reserve(nop);
    atoms.assign(atomitos.begin(), atomitos.end());

    //now assign ghost and real atoms
    int tg = 0;
    int tl = 0;

    for(int i=0; i<nop; i++){
        if(atoms[i].ghost==0){
            tl++;
        }
        else{
            tg++;
        }
    }

    ghost_nop = tg;
    real_nop = tl;
    //cout<<"Assigned real "<<tl<<" ghost "<<tg<<endl;
    //cout<<nop<<endl;

}


//this function allows for handling custom formats of atoms and so on
vector<Atom> System::get_atoms( ){
    //here, we have to filter ghost atoms
    vector<Atom> retatoms;
    for(int i=0; i<real_nop; i++){
        retatoms.emplace_back(atoms[i]);
    }
    return retatoms;

}

void System::add_atoms(vector<Atom> atomitos){

    

    //check for ghost atoms
    int tg = 0;
    int tl = 0;

    for(int i=0; i<atomitos.size(); i++){
        if(atomitos[i].ghost==0){
            tl++;
        }
        else{
            tg++;
        }
    }

    //if there is are no ghosts in system and list, just add and forget
    if(ghost_nop==0){
        if(tg==0){
            for (int i=0; i<atomitos.size(); i++){
                atoms.emplace_back(atomitos[i]);
            }            
        }
    }
    else if (ghost_nop>0){
        //now the atoms need to be reordered
        vector<Atom> real_atoms;
        vector<Atom> ghost_atoms;        
        
        for(int i=0; i<nop; i++){
            if(atoms[i].ghost==0){
                real_atoms.emplace_back(atoms[i]);
            }
            else{
                ghost_atoms.emplace_back(atoms[i]);
            }
        }
        //now also add new atoms to the list
        for (int i=0; i<atomitos.size(); i++){
            if(atomitos[i].ghost==0){
                real_atoms.emplace_back(atomitos[i]);
            }
            else{
                ghost_atoms.emplace_back(atomitos[i]);
            }            
        }
        //now put them all in the big list
        atoms.clear();

        for(int i=0; i<real_atoms.size(); i++){
            atoms.emplace_back(real_atoms[i]);
        }
        for(int i=0; i<ghost_atoms.size(); i++){
            atoms.emplace_back(ghost_atoms[i]);
        }

        //we can clear now
        real_atoms.clear();
        ghost_atoms.clear();
    }

    //update 
    ghost_nop = ghost_nop + tg;
    real_nop = real_nop + tl;
    nop = nop + atomitos.size();
}


vector<Atom> System::get_all_atoms( ){
    //here, we have to filter ghost atoms
    vector<Atom> retatoms;
    for(int i=0; i<nop; i++){
        retatoms.emplace_back(atoms[i]);
    }
    return retatoms;

}

Atom System::gatom(int i) { return atoms[i]; }
void System::satom(Atom atom1) {
    int idd = atom1.loc;
    atoms[idd] = atom1;
}

//----------------------------------------------------
// Neighbor methods
//----------------------------------------------------
double System::get_angle(int ti ,int tj,int tk){
    vector<double> a, b;
    double a_abs, b_abs, adotb, theta;
    a = get_distance_vector(atoms[tj], atoms[ti]);
    b = get_distance_vector(atoms[tk], atoms[ti]);
    a_abs = pow((a[0] * a[0] + a[1] * a[1] + a[2] * a[2]), 0.5);
    b_abs = pow((b[0] * b[0] + b[1] * b[1] + b[2] * b[2]), 0.5);
    adotb = a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
    theta = acos(adotb / a_abs / b_abs);
    return theta;
    return a[0];
}
double System::get_abs_distance(int ti ,int tj,double &diffx ,double &diffy,double &diffz){
    //这东西算triclinic 的最短距离是有问题的，有的点即使在八分之一胞内，又可能距离不是最近的，可能平移之后更近
    //这个函数主要用在两个地方，一个是搜neighbor，一个是算pdf
    //考虑在他在py读取文件的时候给扩胞了，如果扩的胞与所计算的长度范围相比足够大，则在计算neighbor 的时候不会出现问题
    //但是在计算pdf 的时候，关于triclinic距离计算不准的问题无论如何都会产生的。
    //因此在计算pdf的时候如果晶胞是三斜的，则我将包含此函数的算法禁用。

    double abs, ax, ay, az;
    diffx = atoms[tj].posx - atoms[ti].posx;
    diffy = atoms[tj].posy - atoms[ti].posy;
    diffz = atoms[tj].posz - atoms[ti].posz;

    if (triclinic == 1){

        //convert to the triclinic system
        ax = rotinv[0][0]*diffx + rotinv[0][1]*diffy + rotinv[0][2]*diffz;
        ay = rotinv[1][0]*diffx + rotinv[1][1]*diffy + rotinv[1][2]*diffz;
        az = rotinv[2][0]*diffx + rotinv[2][1]*diffy + rotinv[2][2]*diffz;

        //scale to match the triclinic box size
        diffx = ax*boxx;
        diffy = ay*boxy;
        diffz = az*boxz;

        //now check pbc
        //nearest image
        if (diffx> boxx/2.0) {diffx-=boxx;};
        if (diffx<-boxx/2.0) {diffx+=boxx;};
        if (diffy> boxy/2.0) {diffy-=boxy;};
        if (diffy<-boxy/2.0) {diffy+=boxy;};
        if (diffz> boxz/2.0) {diffz-=boxz;};
        if (diffz<-boxz/2.0) {diffz+=boxz;};

        //now divide by box vals - scale down the size
        diffx = diffx/boxx;
        diffy = diffy/boxy;
        diffz = diffz/boxz;

        //now transform back to normal system
        ax = rot[0][0]*diffx + rot[0][1]*diffy + rot[0][2]*diffz;
        ay = rot[1][0]*diffx + rot[1][1]*diffy + rot[1][2]*diffz;
        az = rot[2][0]*diffx + rot[2][1]*diffy + rot[2][2]*diffz;

        //now assign to diffs and calculate distnace
        diffx = ax;
        diffy = ay;
        diffz = az;

        //finally distance
        abs = sqrt(diffx*diffx + diffy*diffy + diffz*diffz);

    }
    else{
        //nearest image
        if (diffx> boxx/2.0) {diffx-=boxx;};
        if (diffx<-boxx/2.0) {diffx+=boxx;};
        if (diffy> boxy/2.0) {diffy-=boxy;};
        if (diffy<-boxy/2.0) {diffy+=boxy;};
        if (diffz> boxz/2.0) {diffz-=boxz;};
        if (diffz<-boxz/2.0) {diffz+=boxz;};
        abs = sqrt(diffx*diffx + diffy*diffy + diffz*diffz);
    }
    return abs;
}

//function for binding
double System::get_abs_distance(Atom atom1 , Atom atom2 ){

    double abs, ax, ay, az;
    double diffx = atom1.posx - atom2.posx;
    double diffy = atom1.posy - atom2.posy;
    double diffz = atom1.posz - atom2.posz;

    if (triclinic == 1){

        //convert to the triclinic system
        ax = rotinv[0][0]*diffx + rotinv[0][1]*diffy + rotinv[0][2]*diffz;
        ay = rotinv[1][0]*diffx + rotinv[1][1]*diffy + rotinv[1][2]*diffz;
        az = rotinv[2][0]*diffx + rotinv[2][1]*diffy + rotinv[2][2]*diffz;

        //scale to match the triclinic box size
        diffx = ax*boxx;
        diffy = ay*boxy;
        diffz = az*boxz;

        //now check pbc
        //nearest image
        if (diffx> boxx/2.0) {diffx-=boxx;};
        if (diffx<-boxx/2.0) {diffx+=boxx;};
        if (diffy> boxy/2.0) {diffy-=boxy;};
        if (diffy<-boxy/2.0) {diffy+=boxy;};
        if (diffz> boxz/2.0) {diffz-=boxz;};
        if (diffz<-boxz/2.0) {diffz+=boxz;};

        //now divide by box vals - scale down the size
        diffx = diffx/boxx;
        diffy = diffy/boxy;
        diffz = diffz/boxz;

        //now transform back to normal system
        ax = rot[0][0]*diffx + rot[0][1]*diffy + rot[0][2]*diffz;
        ay = rot[1][0]*diffx + rot[1][1]*diffy + rot[1][2]*diffz;
        az = rot[2][0]*diffx + rot[2][1]*diffy + rot[2][2]*diffz;

        //now assign to diffs and calculate distnace
        diffx = ax;
        diffy = ay;
        diffz = az;

        //finally distance
        abs = sqrt(diffx*diffx + diffy*diffy + diffz*diffz);

    }
    else{

        //nearest image
        if (diffx> boxx/2.0) {diffx-=boxx;};
        if (diffx<-boxx/2.0) {diffx+=boxx;};
        if (diffy> boxy/2.0) {diffy-=boxy;};
        if (diffy<-boxy/2.0) {diffy+=boxy;};
        if (diffz> boxz/2.0) {diffz-=boxz;};
        if (diffz<-boxz/2.0) {diffz+=boxz;};
        abs = sqrt(diffx*diffx + diffy*diffy + diffz*diffz);
    }

    return abs;
}

//function for binding
vector<double> System::get_distance_vector(Atom atom1 , Atom atom2 ){

    double ax, ay, az;
    double diffx = atom1.posx - atom2.posx;
    double diffy = atom1.posy - atom2.posy;
    double diffz = atom1.posz - atom2.posz;

    if (triclinic == 1){

        //convert to the triclinic system
        ax = rotinv[0][0]*diffx + rotinv[0][1]*diffy + rotinv[0][2]*diffz;
        ay = rotinv[1][0]*diffx + rotinv[1][1]*diffy + rotinv[1][2]*diffz;
        az = rotinv[2][0]*diffx + rotinv[2][1]*diffy + rotinv[2][2]*diffz;

      double dummy;
        //scale to match the triclinic box size
        diffx = ax*boxx;
        diffy = ay*boxy;
        diffz = az*boxz;

        //now check pbc
        //nearest image
        if (diffx> boxx/2.0) {diffx-=boxx;};
        if (diffx<-boxx/2.0) {diffx+=boxx;};
        if (diffy> boxy/2.0) {diffy-=boxy;};
        if (diffy<-boxy/2.0) {diffy+=boxy;};
        if (diffz> boxz/2.0) {diffz-=boxz;};
        if (diffz<-boxz/2.0) {diffz+=boxz;};

        //now divide by box vals - scale down the size
        diffx = diffx/boxx;
        diffy = diffy/boxy;
        diffz = diffz/boxz;

        //now transform back to normal system
        ax = rot[0][0]*diffx + rot[0][1]*diffy + rot[0][2]*diffz;
        ay = rot[1][0]*diffx + rot[1][1]*diffy + rot[1][2]*diffz;
        az = rot[2][0]*diffx + rot[2][1]*diffy + rot[2][2]*diffz;

        //now assign to diffs and calculate distnace
        diffx = ax;
        diffy = ay;
        diffz = az;

    }
    else{

        //nearest image
        if (diffx> boxx/2.0) {diffx-=boxx;};
        if (diffx<-boxx/2.0) {diffx+=boxx;};
        if (diffy> boxy/2.0) {diffy-=boxy;};
        if (diffy<-boxy/2.0) {diffy+=boxy;};
        if (diffz> boxz/2.0) {diffz-=boxz;};
        if (diffz<-boxz/2.0) {diffz+=boxz;};

    }

    vector<double> abs;
    abs.emplace_back(diffx);
    abs.emplace_back(diffy);
    abs.emplace_back(diffz);

    return abs;
}


void System::reset_all_neighbors(vector<int> atomlist){
    int ti=0;
    if (atomlist.size()==0)
    {
        atomlist.resize(nop); 
        for (int i = 0; i < nop; ++i)
        {
            atomlist[i] = i; 
        }
    }
    for (vector<int>::iterator it = atomlist.begin();it != atomlist.end();it++){
        ti = *it;

        atoms[ti].n_neighbors=0;
        atoms[ti].temp_neighbors.clear();
        atoms[ti].condition = 0;

        for (int tn = 0;tn<MAXNUMBEROFNEIGHBORS;tn++){

            atoms[ti].neighbors[tn] = NILVALUE;
            atoms[ti].neighbordist[tn] = -1.0;
        }
    }
}

void System::reset_main_neighbors(){
    for (int ti = 0;ti<nop;ti++){

        atoms[ti].n_neighbors=0;
        atoms[ti].condition = 0;
        //atoms[ti].temp_neighbors.clear();

        for (int tn = 0;tn<MAXNUMBEROFNEIGHBORS;tn++){

            atoms[ti].neighbors[tn] = NILVALUE;
            atoms[ti].neighbordist[tn] = -1.0;
        }
    }
}


vector<int> System::get_pairdistances(double cut,bool partial,int centertype,int secondtype,int histnum,double histlow,int threadnum){
/*

    1，这整套程序没有考虑一个cutoff球大于晶胞内切球的情况。他是通过在最开始初始化atoms的时候对于太小的晶胞扩到几千原子的晶胞来避免这个问题，对于cutoff搜索neighbor算法，由于一般的cutoff比较小，所以不出问题。
    但是这里面存在一个问题，初始化atoms的时候对于正交晶胞我改写了一下算法，让他尽可能扩成一个正方的晶胞，但是对于三斜晶胞，这个扩胞方式还应该再写写。
    2，在计算pdf的时候由于需要考虑长程，用来获得精细的结构因子我在程序中考虑了cutoff球大于晶胞内切球的情况，同时引入多线程：
        对于非partial情况，同时cutoff球内切晶胞的情况，不扩胞，对原胞进行周期性复用，同时开启halftimes优化
        对于partial情况，或者cutoff球大于晶胞内切球的情况，扩胞，此时无法采取对原晶胞周期性复用。原则上partial的cutoff球小于晶胞内切球也可以采用原胞复用，只不过不能开启halftimes，这个还没单独写。
*/


    
    //vector<int> threadflag(threadnum,0);

    pdfpara s;
    s.res = vector<int>(histnum, 0);
    s.resthread = vector<vector<int>>(threadnum,vector<int>(histnum, 0));
    s.deltacut=(cut-histlow)/histnum;
    s.histlow_square=histlow*histlow;
    s.cut_square=cut*cut;
    s.cut=cut;
    s.partial=partial;
    s.centertype=centertype;
    s.secondtype=secondtype;
    s.histnum=histnum;
    s.histlow=histlow;
    //线程相关

    s.threadnum=threadnum;
    if (threadnum>1)
    {
        s.threadflag=(bool*)malloc(threadnum*sizeof(bool));
        memset(s.threadflag, 0, threadnum * sizeof(bool));
    }
    int threadatomsper = (nop / threadnum);
    vector<int> threadatoms(threadnum, threadatomsper);
    if (threadnum * threadatomsper < nop)
    {  
        int t = nop - threadnum * threadatomsper;
        for (int i = 0; i < t; i++)
        {  
            threadatoms[i]++;
        }
    }
    int atomsstart=0;
    //计算平行六面体的高

    if(triclinic==1)
    {
        for(auto &m:s.index)
        {
            int k= m[0];
            int i =m[1];
            int j =m[2];
            for(auto &m2:s.index)
            {
                int k2= m2[0];
                int i2 =m2[1];
                int j2 =m2[2];
                s.iCrossj[k][k2]=box[i][i2]*box[j][j2]-box[i][j2]*box[j][i2];
            }
        }
        
        for(int i=0;i<3;i++)
        {
            for(int j=0;j<3;j++)
            {
            s.iCrossjnorm[i]+= s.iCrossj[i][j]*s.iCrossj[i][j];
            s.kdotiCrossj[i]+=box[i][j]*s.iCrossj[i][j];
            }
            s.iCrossjnorm[i]=pow(s.iCrossjnorm[i],0.5);
            s.Height[i]=abs(s.kdotiCrossj[i]/s.iCrossjnorm[i]);
        }
    }
    else if(triclinic==0)
    {   s.Height[0]=boxx;
        s.Height[1]=boxy;
        s.Height[2]=boxz;
    }
    if(partial==false && (cut/s.Height[0]<0.5) && (cut/s.Height[1]<0.5) &&(cut/s.Height[2]<0.5)){s.halftimes=true;}//开启半数优化
    
    

    
    /*if (s.halftimes == false) {
        int threadid = 0;
        for (int ti=0; ti<nop; ti++){
            if(partial==true && atoms[ti].type!=centertype) { continue; }
            //计算对于每个中心原子应该扩胞的范围
            while(true){
                pdfthreadflaglock.lock();
                if (s.threadflag[threadid] == 0)
                {
                    s.threadflag[threadid] = 1;
                    pdfthreadflaglock.unlock();
                    break;
                }
                pdfthreadflaglock.unlock();
                threadid++;
                if(threadid==s.threadnum){threadid=0;}
            }
            thread theadcal(pairditancethread,ti,threadid,this,&s);
            theadcal.detach();
        }      
    }*/


    /*
    if(cut==0)
    {
        for (int ti=0; ti<nop; ti++){
            for (int tj=ti; tj<nop; tj++){
                if(ti==tj) { continue; }
                d = get_abs_distance(ti,tj,diffx,diffy,diffz);
                res.emplace_back(d);

            }
        }
    }
    */
    if(threadnum>1){
        for(int threadid=0;threadid<threadnum;threadid++)
        {
            s.threadflag[threadid] = 1;
            thread theadcal(pairditancethread,atomsstart, atomsstart+threadatoms[threadid],threadid,this,&s);
            atomsstart += threadatoms[threadid];
            theadcal.detach();
            
        }
        // 等待所有线程结束

        while (true) {
            int threadramainnum = 0;
            pdfthreadflaglock.lock();
            for (int i = 0; i < s.threadnum; i++)
            {
                if (s.threadflag[i] == 1)
                {
                    threadramainnum++;
                }
            }
            pdfthreadflaglock.unlock();
            if (threadramainnum==0) { break; }
        }
        for (int i = 0; i < histnum; i++)
        {
            for (int j = 0; j < threadnum; j++)
            {
                s.res[i] += s.resthread[j][i];
            }
        }
        free(s.threadflag);
        pdf_halftimes=s.halftimes;
        return s.res;
    }
    else if(threadnum==1)
    {   
        int threadid=0;
        thread theadcal(pairditancethread,atomsstart, atomsstart+threadatoms[threadid],threadid,this,&s);
        //atomsstart += threadatoms[threadid];
        theadcal.join();
        //s.threadflag[threadid] = 1;
        //free(s.threadflag);
        pdf_halftimes=s.halftimes;
        return s.resthread[threadid];
    }

}

void System::pairditancethread(int atomsstart,int atomsfinish, int threadid,System* sys,pdfpara * s){

    double d_square,d;
    double diffx,diffy,diffz;
    int M[3]={0};
    int N[3]={0};
    double pointHeight[3]={0};
    double pdotiCrossj[3]={0};


    if (s->halftimes == true) {
        for (int ti = atomsstart; ti < atomsfinish; ti++) {
            for (int tj = ti + 1; tj < sys->nop; tj++) {
                d=sys->get_abs_distance(ti, tj, diffx, diffy, diffz);
                //d_square = diffx * diffx + diffy * diffy + diffz * diffz;
                //if (d_square <= s->cut_square && d_square >= s->histlow_square) {
                if (d <= s->cut && d >= s->histlow) {
                    //d = pow(d_square, 0.5);
                    
                    s->resthread[threadid][floor((d - s->histlow) / s->deltacut)]++;
                    
                }
            }
        }
    }
    if (s->halftimes==false){
        for (int ti = atomsstart; ti < atomsfinish; ti++) {
            if (s->partial == true && sys->atoms[ti].type != s->centertype) { continue; }
            //计算对于每个中心原子应该扩胞的范围

            if (sys->triclinic == 1) {
                for (int i = 0; i < 3; i++) {
                    pdotiCrossj[i] += sys->atoms[ti].posx * s->iCrossj[i][0] + sys->atoms[ti].posy * s->iCrossj[i][1] + sys->atoms[ti].posz * s->iCrossj[i][2];
                    pointHeight[i] = abs(s->pdotiCrossj[i] / s->iCrossjnorm[i]);
                    M[i] = ceil((pointHeight[i] + s->cut) / s->Height[i] - 1);
                    N[i] = floor((pointHeight[i] - s->cut) / s->Height[i]);
                }
            }
            else {
                M[0] = ceil((sys->atoms[ti].posx + s->cut) / s->Height[0] - 1);
                N[0] = floor((sys->atoms[ti].posx - s->cut) / s->Height[0]);
                M[1] = ceil((sys->atoms[ti].posy + s->cut) / s->Height[1] - 1);
                N[1] = floor((sys->atoms[ti].posy - s->cut) / s->Height[1]);
                M[2] = ceil((sys->atoms[ti].posz + s->cut) / s->Height[2] - 1);
                N[2] = floor((sys->atoms[ti].posz - s->cut) / s->Height[2]);

            }
            for (int tj = 0; tj < sys->nop; tj++) {
                if (s->partial == true && sys->atoms[tj].type != s->secondtype) { continue; }
                
                for (int i = N[0]; i <= M[0]; i++) {
                    for (int j = N[1]; j <= M[1]; j++) {
                        for (int k = N[2]; k <= M[2]; k++) {
                            if (ti == tj && i==0 && j==0 && k==0) { continue; }
                            diffx = sys->atoms[tj].posx + i * sys->box[0][0] + j * sys->box[1][0] + k * sys->box[2][0] - sys->atoms[ti].posx;
                            diffy = sys->atoms[tj].posy + i * sys->box[0][1] + j * sys->box[1][1] + k * sys->box[2][1] - sys->atoms[ti].posy;
                            diffz = sys->atoms[tj].posz + i * sys->box[0][2] + j * sys->box[1][2] + k * sys->box[2][2] - sys->atoms[ti].posz;

                            d_square = diffx * diffx + diffy * diffy + diffz * diffz;
                            //std::cout << s->cut_square;
                            if (d_square <= s->cut_square && d_square >= s->histlow_square) {
                                d = pow(d_square, 0.5);
                                
                                s->resthread[threadid][floor((d - s->histlow) / s->deltacut)]++;

                            }
                        }
                    }
                }
            }
        }
             
    }
    if (s->threadnum>1)
    {
        sys->pdfthreadflaglock.lock();
        s->threadflag[threadid] = 0;
        sys->pdfthreadflaglock.unlock();
    }
}
vector<int> System::get_pairangle(double histlow,double histhigh,int histnum){

    vector<int> res(histnum,0);
    double delta=(histhigh-histlow)/histnum;
    double d;
    //double diffx,diffy,diffz;

    for (int ti=0; ti<nop; ti++){
        for (int tj=0; tj<atoms[ti].n_neighbors; tj++)
            for (int tk=tj; tk<atoms[ti].n_neighbors; tk++){
                if(tk==tj) { continue; }
                d = get_angle(ti,atoms[ti].neighbors[tj],atoms[ti].neighbors[tk]);
                if(d>=histlow && d<=histhigh)
                {
                    res[floor((d-histlow)/delta)]++;
                }
        }
    }
    return res;
}

//function to create cell lists
//snmall function that returns cell index when provided with cx, cy, cz
int System::cell_index(int cx, int cy, int cz){
    return cx*ny*nz + cy*nz + cz;
}


//if number of particles are small, use brute force
//if box is triclinic, use brute force
void System::set_up_cells(){

      int si,sj,sk, maincell, subcell;
      vector<int> cc;
      //find of all find the number of cells in each direction
      nx = boxx/neighbordistance;
      ny = boxy/neighbordistance;
      nz = boxz/neighbordistance;
      //now use this to find length of cell in each direction
      double lx = boxx/nx;
      double ly = boxy/ny;
      double lz = boxz/nz;
      //find the total number of cells
      total_cells = nx*ny*nz;
      //create a vector of cells
      cells = new cell[total_cells];
      //now run over and for each cell create its neighbor cells
      //all neighbor cells are also added
      for(int i=0; i<nx; i++){
         for(int j=0; j<ny; j++){
           for(int k=0; k<nz; k++){
              maincell = cell_index(i, j, k);
              for(int si=i-1; si<=i+1; si++){
                  for(int sj=j-1; sj<=j+1; sj++){
                      for(int sk=k-1; sk<=k+1; sk++){
                         cc = cell_periodic(si, sj, sk);
                         subcell = cell_index(cc[0], cc[1], cc[2]);
                         //add this to the list of neighbors
                         cells[maincell].neighbor_cells.emplace_back(subcell);

                      }
                  }
              }
           }
         }
      }
      int cx, cy, cz;
      double dx, dy, dz;
      double ddx, ddy, ddz;
      int ind;

      //now loop over all atoms and assign cells
      for(int ti=0; ti<nop; ti++){

          //calculate c indices for the atom
          dx = atoms[ti].posx;
          dy = atoms[ti].posy;
          dz = atoms[ti].posz;
          
          //now apply boxdims
          if( abs(dx-0) < 1E-6)
              dx = 0;
          if( abs(dy-0) < 1E-6)
              dy = 0;
          if( abs(dz-0) < 1E-6)
              dz = 0;

          if (dx < 0) dx+=boxx;
          else if (dx >= boxx) dx-=boxx;
          if (dy < 0) dy+=boxy;
          else if (dy >= boxy) dy-=boxy;
          if (dz < 0) dz+=boxz;
          else if (dz >= boxz) dz-=boxz;

          //now find c vals
          cx = dx/lx;
          cy = dy/ly;
          cz = dz/lz;

          //now get cell index
          ind = cell_index(cx, cy, cz);
          //got cell index
          //now add the atom to the corresponding cells
          cells[ind].members.emplace_back(ti);

      }
      //end of loop - all cells, the member atoms and neighboring cells are added
}

vector<double> System::remap_atom(vector<double> pos){
    //remap atom position into the box
    //now apply boxdims
    double dx = pos[0];
    double dy = pos[1];
    double dz = pos[2];
    double ax, ay, az;

    if (triclinic == 1){
        //convert to the triclinic system
        ax = rotinv[0][0]*dx + rotinv[0][1]*dy + rotinv[0][2]*dz;
        ay = rotinv[1][0]*dx + rotinv[1][1]*dy + rotinv[1][2]*dz;
        az = rotinv[2][0]*dx + rotinv[2][1]*dy + rotinv[2][2]*dz;

        //scale to match the triclinic box size
        dx = ax*boxx;
        dy = ay*boxy;
        dz = az*boxz;

        //now check pbc
        //nearest image
        if (dx> boxx/2.0) {dx-=boxx;};
        if (dx<-boxx/2.0) {dx+=boxx;};
        if (dy> boxy/2.0) {dy-=boxy;};
        if (dy<-boxy/2.0) {dy+=boxy;};
        if (dz> boxz/2.0) {dz-=boxz;};
        if (dz<-boxz/2.0) {dz+=boxz;};

        //now divide by box vals - scale down the size
        dx = dx/boxx;
        dy = dy/boxy;
        dz = dz/boxz;

        //now transform back to normal system
        ax = rot[0][0]*dx + rot[0][1]*dy + rot[0][2]*dz;
        ay = rot[1][0]*dx + rot[1][1]*dy + rot[1][2]*dz;
        az = rot[2][0]*dx + rot[2][1]*dy + rot[2][2]*dz;

        //now assign to diffs and calculate distnace
        dx = ax;
        dy = ay;
        dz = az;
    }
    else{
        if (dx < 0) dx+=boxx;
        else if (dx >= boxx) dx-=boxx;
        if (dy < 0) dy+=boxy;
        else if (dy >= boxy) dy-=boxy;
        if (dz < 0) dz+=boxz;
        else if (dz >= boxz) dz-=boxz;
    }
    vector<double> rpos;
    rpos.emplace_back(dx);
    rpos.emplace_back(dy);
    rpos.emplace_back(dz);
    return rpos;

}

vector<int> System::cell_periodic(int i, int j, int k){
    vector<int> ci;
    //apply periodic conditions
    if (i<0) i = i + nx;
    else if (i>nx-1) i = i -nx;
    ci.emplace_back(i);
    if (j<0) j = j + ny;
    else if (j>ny-1) j = j -ny;
    ci.emplace_back(j);
    if (k<0) k = k + nz;
    else if (k>nz-1) k = k -nz;
    ci.emplace_back(k);
    return ci;

}

//get all neighbor info but using cell lists
void System::get_all_neighbors_cells(){

    voronoiused = 0;

    double d;
    double diffx,diffy,diffz;
    double r,theta,phi;
    int ti, tj;
    //first create cells
    set_up_cells();
    int maincell, subcell;

    //now loop to find distance
    for(int i=0; i<total_cells; i++){
        //now go over the neighbor cells
        //for each member in cell i
        for(int mi=0; mi<cells[i].members.size(); mi++){
            //now go through the neighbors
            ti = cells[i].members[mi];
            for(int j=0 ; j<cells[i].neighbor_cells.size(); j++){
               //loop through members of j
               subcell = cells[i].neighbor_cells[j];
               for(int mj=0; mj<cells[subcell].members.size(); mj++){
                  //now we have mj -> members/compare with
                  tj = cells[subcell].members[mj];
                  //compare ti and tj and add
                  if (ti < tj){
                      d = get_abs_distance(ti,tj,diffx,diffy,diffz);
                      if (d < neighbordistance){

                        if ((filter == 1) && (atoms[ti].type != atoms[tj].type)){
                            continue;
                        }
                        else if ((filter == 2) && (atoms[ti].type == atoms[tj].type)){
                            continue;
                        }
                        //process_neighbor(ti, tj);
                        atoms[ti].neighbors[atoms[ti].n_neighbors] = tj;
                        atoms[ti].neighbordist[atoms[ti].n_neighbors] = d;
                        //weight is set to 1.0, unless manually reset
                        atoms[ti].neighborweight[atoms[ti].n_neighbors] = 1.00;
                        atoms[ti].n_diffx[atoms[ti].n_neighbors] = diffx;
                        atoms[ti].n_diffy[atoms[ti].n_neighbors] = diffy;
                        atoms[ti].n_diffz[atoms[ti].n_neighbors] = diffz;
                        convert_to_spherical_coordinates(diffx, diffy, diffz, r, phi, theta);
                        atoms[ti].n_r[atoms[ti].n_neighbors] = r;
                        atoms[ti].n_phi[atoms[ti].n_neighbors] = phi;
                        atoms[ti].n_theta[atoms[ti].n_neighbors] = theta;
                        atoms[ti].n_neighbors += 1;
                        atoms[ti].cutoff = neighbordistance;

                        atoms[tj].neighbors[atoms[tj].n_neighbors] = ti;
                        atoms[tj].neighbordist[atoms[tj].n_neighbors] = d;
                        //weight is set to 1.0, unless manually reset
                        atoms[tj].neighborweight[atoms[tj].n_neighbors] = 1.00;
                        atoms[tj].n_diffx[atoms[tj].n_neighbors] = -diffx;
                        atoms[tj].n_diffy[atoms[tj].n_neighbors] = -diffy;
                        atoms[tj].n_diffz[atoms[tj].n_neighbors] = -diffz;
                        convert_to_spherical_coordinates(-diffx, -diffy, -diffz, r, phi, theta);
                        atoms[tj].n_r[atoms[tj].n_neighbors] = r;
                        atoms[tj].n_phi[atoms[tj].n_neighbors] = phi;
                        atoms[tj].n_theta[atoms[tj].n_neighbors] = theta;
                        atoms[tj].n_neighbors +=1;
                        atoms[tj].cutoff = neighbordistance;
                      }
                  }
               }

            }

        }
    }


}



void System::get_all_neighbors_normal(){


    //reset voronoi flag
    voronoiused = 0;

    double d;
    double diffx,diffy,diffz;
    double r,theta,phi;

    for (int ti=0; ti<nop; ti++){
        for (int tj=ti; tj<nop; tj++){
            if(ti==tj) { continue; }

            d = get_abs_distance(ti,tj,diffx,diffy,diffz);
            if (d < neighbordistance){
                if ((filter == 1) && (atoms[ti].type != atoms[tj].type)){
                    continue;
                }
                else if ((filter == 2) && (atoms[ti].type == atoms[tj].type)){
                    continue;
                }
                //process_neighbor(ti, tj);
                atoms[ti].neighbors[atoms[ti].n_neighbors] = tj;
                atoms[ti].neighbordist[atoms[ti].n_neighbors] = d;
                //weight is set to 1.0, unless manually reset
                atoms[ti].neighborweight[atoms[ti].n_neighbors] = 1.00;
                atoms[ti].n_diffx[atoms[ti].n_neighbors] = diffx;
                atoms[ti].n_diffy[atoms[ti].n_neighbors] = diffy;
                atoms[ti].n_diffz[atoms[ti].n_neighbors] = diffz;
                convert_to_spherical_coordinates(diffx, diffy, diffz, r, phi, theta);
                atoms[ti].n_r[atoms[ti].n_neighbors] = r;
                atoms[ti].n_phi[atoms[ti].n_neighbors] = phi;
                atoms[ti].n_theta[atoms[ti].n_neighbors] = theta;
                atoms[ti].n_neighbors += 1;
                atoms[ti].cutoff = neighbordistance;

                atoms[tj].neighbors[atoms[tj].n_neighbors] = ti;
                atoms[tj].neighbordist[atoms[tj].n_neighbors] = d;
                //weight is set to 1.0, unless manually reset
                atoms[tj].neighborweight[atoms[tj].n_neighbors] = 1.00;
                atoms[tj].n_diffx[atoms[tj].n_neighbors] = -diffx;
                atoms[tj].n_diffy[atoms[tj].n_neighbors] = -diffy;
                atoms[tj].n_diffz[atoms[tj].n_neighbors] = -diffz;
                convert_to_spherical_coordinates(-diffx, -diffy, -diffz, r, phi, theta);
                atoms[tj].n_r[atoms[tj].n_neighbors] = r;
                atoms[tj].n_phi[atoms[tj].n_neighbors] = phi;
                atoms[tj].n_theta[atoms[tj].n_neighbors] = theta;
                atoms[tj].n_neighbors +=1;
                atoms[tj].cutoff = neighbordistance;
            }
        }

    }


}

void System::process_neighbor(int ti, int tj){
    /*
    Calculate all info and add it to list
    ti - loc of host atom
    tj - loc of the neighbor
     d - interatomic distance
     */

    double d, diffx, diffy, diffz;
    double r, phi, theta;

    d = get_abs_distance(ti, tj, diffx,diffy,diffz);

    atoms[ti].neighbors[atoms[ti].n_neighbors] = tj;
    atoms[ti].neighbordist[atoms[ti].n_neighbors] = d;
    //weight is set to 1.0, unless manually reset
    atoms[ti].neighborweight[atoms[ti].n_neighbors] = 1.00;
    atoms[ti].n_diffx[atoms[ti].n_neighbors] = diffx;
    atoms[ti].n_diffy[atoms[ti].n_neighbors] = diffy;
    atoms[ti].n_diffz[atoms[ti].n_neighbors] = diffz;
    convert_to_spherical_coordinates(diffx, diffy, diffz, r, phi, theta);
    atoms[ti].n_r[atoms[ti].n_neighbors] = r;
    atoms[ti].n_phi[atoms[ti].n_neighbors] = phi;
    atoms[ti].n_theta[atoms[ti].n_neighbors] = theta;
    atoms[ti].n_neighbors += 1;

}

/*
To increase the speed of the other methods, we need some functions using cells and
otheriwse which adds atoms to the temp_neighbors list
*/
void System::get_temp_neighbors_brute(vector<int> atomlist){

    //reset voronoi flag

    double d;
    double diffx,diffy,diffz;
    int ti=0;
    bool halftime;
    if (atomlist.size()==0)
    {
        atomlist.resize(nop); 
        for (int i = 0; i < nop; ++i)
        {
            atomlist[i] = i; 
        }
        halftime=true;
    }
    else{
        halftime=false;
    }
    for (vector<int>::iterator it = atomlist.begin();it != atomlist.end();it++){
        ti = *it;
        if(halftime){
            for (int tj=ti; tj<nop; tj++){
                if(ti==tj) { continue; }
                d = get_abs_distance(ti,tj,diffx,diffy,diffz);
                if (d <= neighbordistance){
                    datom x = {d, tj};
                    atoms[ti].temp_neighbors.emplace_back(x);
                    datom y = {d, ti};
                    atoms[tj].temp_neighbors.emplace_back(y);
                }
            }
        }
        else{
            for (int tj=0; tj<nop; tj++){
                if(ti==tj) { continue; }
                d = get_abs_distance(ti,tj,diffx,diffy,diffz);
                if (d <= neighbordistance){
                    datom x = {d, tj};
                    atoms[ti].temp_neighbors.emplace_back(x);
                    //datom y = {d, ti};
                    //atoms[tj].temp_neighbors.emplace_back(y);
                }
            }
        }
    }

}

/*
Cells should only be used when the system has a minimum size - in this case,
about 2000 atoms.
*/
void System::get_temp_neighbors_cells(vector<int> atomlist){

    //first create cells
    set_up_cells();

    int maincell, subcell;
    int ti, tj;
    int i;
    double d;
    double diffx,diffy,diffz;
    bool halftime;
    vector<int> cellforatom;
    cellforatom.resize(nop);
    if (atomlist.size()==0)
    {
        atomlist.resize(nop); 
        for (int i = 0; i < nop; ++i)
        {
            atomlist[i] = i; 
        }
        halftime=true;
    }
    else{
        halftime=false;
    }
    for(int i=0; i<total_cells; i++){
        for(int mi=0; mi<cells[i].members.size(); mi++)
        {
            ti = cells[i].members[mi];
            cellforatom[ti]=i;
        }
    }

    //now loop to find distance
    //for(int i=0; i<total_cells; i++){
        //now go over the neighbor cells
        //for each member in cell i
        //for(int mi=0; mi<cells[i].members.size(); mi++){
            //now go through the neighbors
            //ti = cells[i].members[mi];
        for (vector<int>::iterator it = atomlist.begin();it != atomlist.end();it++){
            ti = *it;
            i=cellforatom[ti];
            for(int j=0 ; j<cells[i].neighbor_cells.size(); j++){
               //loop through members of j
               subcell = cells[i].neighbor_cells[j];
               for(int mj=0; mj<cells[subcell].members.size(); mj++){
                    //now we have mj -> members/compare with
                    tj = cells[subcell].members[mj];
                    //compare ti and tj and add
                    if(halftime){
                        if (ti < tj){
                            d = get_abs_distance(ti,tj,diffx,diffy,diffz);
                            if (d < neighbordistance){
                                datom x = {d, tj};
                                atoms[ti].temp_neighbors.emplace_back(x);
                                datom y = {d, ti};
                                atoms[tj].temp_neighbors.emplace_back(y);
                            }
                        }
                    }
                    else{
                        if (ti != tj){
                            d = get_abs_distance(ti,tj,diffx,diffy,diffz);
                            if (d < neighbordistance){
                                datom x = {d, tj};
                                atoms[ti].temp_neighbors.emplace_back(x);
                                //datom y = {d, ti};
                                //atoms[tj].temp_neighbors.emplace_back(y);
                            }
                        }
                    }
               }

            }

        }
    //}

}
int System::get_all_neighbors_bynumber(double prefactor, int nns, int assign,vector<int> atomlist){
    /*
    A new neighbor algorithm that finds a specified number of 
    neighbors for each atom. But ONLY TEMP neighbors
    */

    //reset voronoi flag
    voronoiused = 0;
    

    double d, dcut;
    double diffx,diffy,diffz;
    double r,theta,phi;
    int m, maxneighs, finished;
    finished = 1;

    vector<int> nids;
    vector<double> dists, sorted_dists;

        //double prefactor = 1.21;
    double summ;
    double boxvol;
    int ti=0;
    if (atomlist.size()==0)
    {
        atomlist.resize(nop); 
        for (int i = 0; i < nop; ++i)
        {
            atomlist[i] = i; 
        }
    }
    //some guesswork here
    //find the box volumes
    if (triclinic==1){
        double a1, a2, a3, b1, b2, b3, c1, c2, c3;
        //rot is the cell vectors transposed
        a1 = rot[0][0];
        a2 = rot[1][0];
        a3 = rot[2][0];
        b1 = rot[0][1];
        b2 = rot[1][1];
        b3 = rot[2][1];
        c1 = rot[0][2];
        c2 = rot[1][2];
        c3 = rot[2][2];
        boxvol = c1*(a2*b3-a3*b2) - c2*(a1*b3-b1*a3) + c3*(a1*b2-a2*b1);
    }
    else{
        boxvol = boxx*boxy*boxz;
    }

    //now find the volume per particle
    double guessvol = boxvol/float(nop);

    //guess the side of a cube that is occupied by an atom - this is a guess distance
    double guessdist = cbrt(guessvol);

    //now add some safe padding - this is the prefactor which we will read in
    guessdist = prefactor*guessdist;
    neighbordistance = guessdist;

    if (usecells){
        get_temp_neighbors_cells(atomlist);
    }
    else{
        get_temp_neighbors_brute(atomlist);
    }
    for (vector<int>::iterator it = atomlist.begin();it != atomlist.end();it++){
        ti = *it;
        if (atoms[ti].temp_neighbors.size() < nns){
            return 0;
        }

        sort(atoms[ti].temp_neighbors.begin(), atoms[ti].temp_neighbors.end(), by_dist());

        if(assign == 1){
            //assign the neighbors
            for(int i=0; i<nns; i++){
                int tj = atoms[ti].temp_neighbors[i].index;
                process_neighbor(ti, tj);
            }
        }

        finished = 1;            
    }


    return finished;


}

void System::set_atom_cutoff(double factor){
    /*
    Reassign atom cutoff
    */
    int nn;
    double sum;
    double avgdist;

    for (int ti=0; ti<nop; ti++){
        nn = atoms[ti].n_neighbors;
        sum = 0;
        for (int j=0; j<nn; j++){
            sum += atoms[ti].neighbordist[j];
        }
        avgdist = sum/(double(nn));
        atoms[ti].cutoff = factor*avgdist;
    }
}

int System::get_neighbors_from_temp(int style){
    /*
    A new neighbor algorithm that finds a specified number of 
    neighbors for each atom.
    */

    int finished = 1;
    //reset neighbors
    reset_main_neighbors();

    if (style == 12){
        for (int ti=0; ti<nop; ti++){
            if (atoms[ti].temp_neighbors.size() > 11){
                double ssum = 0;
                for(int i=0 ; i<12; i++){
                    ssum += atoms[ti].temp_neighbors[i].dist;
                }
                //process sum
                atoms[ti].lcutsmall = 1.2071*ssum/12;
                //now assign neighbors based on this
                for(int i=0 ; i<atoms[ti].temp_neighbors.size(); i++){
                    int tj = atoms[ti].temp_neighbors[i].index;
                    double dist = atoms[ti].temp_neighbors[i].dist;
                    if (dist <= atoms[ti].lcutsmall)
                        process_neighbor(ti, tj);
                }
                finished = 1;                                  
            }
            else{
                return 0;
            }
        }
    }
    else if (style == 14){
        for (int ti=0; ti<nop; ti++){
            if (atoms[ti].temp_neighbors.size() > 13){
                double ssum = 0;
                for(int i=0 ; i<8; i++){
                    ssum += 1.1547*atoms[ti].temp_neighbors[i].dist;
                }
                for(int i=8 ; i<14; i++){
                    ssum += atoms[ti].temp_neighbors[i].dist;
                }
                atoms[ti].lcutlarge = 1.2071*ssum/14;
                //now assign neighbors based on this
                for(int i=0 ; i<atoms[ti].temp_neighbors.size(); i++){
                    int tj = atoms[ti].temp_neighbors[i].index;
                    double dist = atoms[ti].temp_neighbors[i].dist;
                    if (dist <= atoms[ti].lcutlarge)
                        process_neighbor(ti, tj);
                }
                finished = 1;                                  
            }
            else{
                return 0;
            }
        }
    }


    return finished;


}


void System::store_neighbor_info(){
    /*
    Method to assign neighbors and next nearest neighbors

    */    
    int nn;

    for (int ti=0; ti<nop; ti++){

        atoms[ti].next_neighbors.clear();
        atoms[ti].next_neighbor_distances.clear();
        atoms[ti].next_neighbor_counts.clear();
        
        atoms[ti].next_neighbors.resize(atoms[ti].n_neighbors);
        atoms[ti].next_neighbor_distances.resize(atoms[ti].n_neighbors);
        atoms[ti].next_neighbor_counts.resize(atoms[ti].n_neighbors);

        for(int i=0; i<atoms[ti].n_neighbors; i++){
            nn = atoms[ti].neighbors[i];
            atoms[ti].next_neighbor_counts[i] = atoms[nn].n_neighbors;
            for(int j=0; j<atoms[nn].n_neighbors; j++){
                atoms[ti].next_neighbors[i].emplace_back(atoms[nn].neighbors[j]);
                atoms[ti].next_neighbor_distances[i].emplace_back(atoms[nn].neighbordist[j]);
            }
        }
    }
}




int System::get_all_neighbors_sann(double prefactor){
    /*
    A new adaptive algorithm. Similar to the old ones, we guess a basic distance with padding,
    and sort them up.
    After that, we use the algorithm by in J. Chem. Phys. 136, 234107 (2012) to find the list of
    neighbors.
     */

    //reset voronoi flag
    voronoiused = 0;

    double d, dcut;
    double diffx,diffy,diffz;
    double r,theta,phi;
    int m, maxneighs, finished;
    finished = 1;

    vector<int> nids;
    vector<double> dists, sorted_dists;

    //double prefactor = 1.21;
    double summ;
    double boxvol;

    //some guesswork here
    //find the box volumes
    if (triclinic==1){
        double a1, a2, a3, b1, b2, b3, c1, c2, c3;
        //rot is the cell vectors transposed
        a1 = rot[0][0];
        a2 = rot[1][0];
        a3 = rot[2][0];
        b1 = rot[0][1];
        b2 = rot[1][1];
        b3 = rot[2][1];
        c1 = rot[0][2];
        c2 = rot[1][2];
        c3 = rot[2][2];
        boxvol = c1*(a2*b3-a3*b2) - c2*(a1*b3-b1*a3) + c3*(a1*b2-a2*b1);
    }
    else{
        boxvol = boxx*boxy*boxz;
    }


    //now find the volume per particle
    double guessvol = boxvol/float(nop);

    //guess the side of a cube that is occupied by an atom - this is a guess distance
    double guessdist = cbrt(guessvol);

    //now add some safe padding - this is the prefactor which we will read in
    guessdist = prefactor*guessdist;
    neighbordistance = guessdist;

    if (usecells){
        get_temp_neighbors_cells();
    }
    else{
        get_temp_neighbors_brute();
    }

    for (int ti=0; ti<nop; ti++){
        if (atoms[ti].temp_neighbors.size() < 3){
            return 0;
        }

        sort(atoms[ti].temp_neighbors.begin(), atoms[ti].temp_neighbors.end(), by_dist());

        //start with initial routine
        m = 3;
        summ = 0;
        for(int i=0 ; i<m; i++){
            summ += atoms[ti].temp_neighbors[i].dist;
            int tj = atoms[ti].temp_neighbors[i].index;
            process_neighbor(ti, tj);
        }

        //find cutoff
        dcut = summ/float(m-2);
        maxneighs = atoms[ti].temp_neighbors.size();

        while( (m < maxneighs) && (dcut >= atoms[ti].temp_neighbors[m].dist)){
            //increase m
            m = m+1;

            //here now we can add this to the list neighbors and process things
            int tj = atoms[ti].temp_neighbors[m].index;
            process_neighbor(ti, tj);

            //find new dcut
            summ = summ + atoms[ti].temp_neighbors[m].dist;
            dcut = summ/float(m-2);
            atoms[ti].cutoff = dcut;
        }

        //find if there was an error
        if (m==maxneighs){
            finished = 0;
            break;
        }
        else{
            finished = 1;
        }

    }


    return finished;


}



int System::get_all_neighbors_adaptive(double prefactor, int nlimit, double padding){

    double d, dcut;
    double diffx,diffy,diffz;
    double r,theta,phi;
    int m, maxneighs, finished;

    double summ;
    double boxvol;
    //some guesswork here
    //find the box volumes
    if (triclinic==1){
        double a1, a2, a3, b1, b2, b3, c1, c2, c3;
        //rot is the cell vectors transposed
        a1 = rot[0][0];
        a2 = rot[1][0];
        a3 = rot[2][0];
        b1 = rot[0][1];
        b2 = rot[1][1];
        b3 = rot[2][1];
        c1 = rot[0][2];
        c2 = rot[1][2];
        c3 = rot[2][2];
        boxvol = c1*(a2*b3-a3*b2) - c2*(a1*b3-b1*a3) + c3*(a1*b2-a2*b1);
    }
    else{
        boxvol = boxx*boxy*boxz;
    }

    //now find the volume per particle
    double guessvol = boxvol/float(nop);

    //guess the side of a cube that is occupied by an atom - this is a guess distance
    double guessdist = cbrt(guessvol);

    //now add some safe padding - this is the prefactor which we will read in
    guessdist = prefactor*guessdist;
    neighbordistance = guessdist;

    //introduce cell lists here - instead of looping over all neighbors
    //use cells

    if (usecells){
        get_temp_neighbors_cells();
    }
    else{
        get_temp_neighbors_brute();
    }

    //end of callstructural competition
    //subatoms would now be populated
    //now starts the main loop
    for (int ti=0; ti<nop; ti++){
        //check if its zero size
        if (atoms[ti].temp_neighbors.size() < nlimit){
            return 0;
        }

        sort(atoms[ti].temp_neighbors.begin(), atoms[ti].temp_neighbors.end(), by_dist());

        summ = 0;
        for(int i=0; i<nlimit; i++){
            summ += atoms[ti].temp_neighbors[i].dist;
        }
        dcut = padding*(1.0/float(nlimit))*summ;

        //now we are ready to loop over again, but over the lists
        for(int j=0; j<atoms[ti].temp_neighbors.size(); j++){
            int tj = atoms[ti].temp_neighbors[j].index;
            if (atoms[ti].temp_neighbors[j].dist < dcut){

                if ((filter == 1) && (atoms[ti].type != atoms[tj].type)){
                    continue;
                }
                else if ((filter == 2) && (atoms[ti].type == atoms[tj].type)){
                    continue;
                }
                d = get_abs_distance(ti,tj,diffx,diffy,diffz);
                atoms[ti].neighbors[atoms[ti].n_neighbors] = tj;
                atoms[ti].neighbordist[atoms[ti].n_neighbors] =d;
                //weight is set to 1.0, unless manually reset
                atoms[ti].neighborweight[atoms[ti].n_neighbors] = 1.00;
                atoms[ti].n_diffx[atoms[ti].n_neighbors] = diffx;
                atoms[ti].n_diffy[atoms[ti].n_neighbors] = diffy;
                atoms[ti].n_diffz[atoms[ti].n_neighbors] = diffz;
                convert_to_spherical_coordinates(diffx, diffy, diffz, r, phi, theta);
                atoms[ti].n_r[atoms[ti].n_neighbors] = r;
                atoms[ti].n_phi[atoms[ti].n_neighbors] = phi;
                atoms[ti].n_theta[atoms[ti].n_neighbors] = theta;
                atoms[ti].n_neighbors += 1;
                atoms[ti].cutoff = dcut;

            }
        }

    }


    return 1;

}

void System::set_neighbordistance(double nn) { neighbordistance = nn; }


//---------------------------------------------------
// Methods for q calculation
//---------------------------------------------------
double System::dfactorial(int l,int m){

    double fac = 1.00;
    for(int i=0;i<2*m;i++){
        fac*=double(l+m-i);
    }
    return (1.00/fac);
}

void System::set_reqd_qs(vector <int> qs){

    lenqs = qs.size();
    reqdqs = new int[lenqs];
    for(int i=0;i<lenqs;i++){
        reqdqs[i] = qs[i];
    }

    rq_backup = qs;
}


void System::set_reqd_aqs(vector <int> qs){

    lenaqs = qs.size();
    reqdaqs = new int[lenaqs];
    for(int i=0;i<lenaqs;i++){
        for(int j=0;j<lenqs;j++){
            if(qs[i]==reqdqs[j]) { reqdaqs[i] = qs[i]; }
        }
    }
    //only qvlaues in the normal set will be included in the aq list
    //check here if its in the qlist
    //cout<<"corresponding q value should also be set."<<endl;

}

double System::PLM(int l, int m, double x){

    double fact,pll,pmm,pmmp1,somx2;
    int i,ll;
    pll = 0.0;
    if (m < 0 || m > l || fabs(x) > 1.0)
        cerr << "impossible combination of l and m" << "\n";
    pmm=1.0;
    if (m > 0){
        somx2=sqrt((1.0-x)*(1.0+x));
        fact=1.0;
        for (i=1;i<=m;i++){
            pmm *= -fact*somx2;
            fact += 2.0;
        }
    }

    if (l == m)
        return pmm;
    else{
        pmmp1=x*(2*m+1)*pmm;
        if (l == (m+1))
            return pmmp1;
        else{
            for (ll=m+2;ll<=l;ll++){
            pll=(x*(2*ll-1)*pmmp1-(ll+m-1)*pmm)/(ll-m);
            pmm=pmmp1;
            pmmp1=pll;
            }
        return pll;
        }
    }
}

void System::convert_to_spherical_coordinates(double x, double y, double z, double &r, double &phi, double &theta){
    r = sqrt(x*x+y*y+z*z);
    theta = acos(z/r);
    phi = atan2(y,x);
}


void System::YLM(int l, int m, double theta, double phi, double &realYLM, double &imgYLM){

    double factor;
    double m_PLM;
    m_PLM = PLM(l,m,cos(theta));
    factor = ((2.0*double(l) + 1.0)/ (4.0*PI))*dfactorial(l,m);
    realYLM = sqrt(factor) * m_PLM * cos(double(m)*phi);
    imgYLM  = sqrt(factor) * m_PLM * sin(double(m)*phi);
}


void System::QLM(int l,int m,double theta,double phi,double &realYLM, double &imgYLM ){

    realYLM = 0.0;
    imgYLM = 0.0;
    if (m < 0) {
        YLM(l, abs(m), theta, phi, realYLM, imgYLM);
        realYLM = pow(-1.0,m)*realYLM;
        imgYLM = pow(-1.0,m+1)*imgYLM;
    }
    else{
        YLM(l, m, theta, phi, realYLM, imgYLM);
    }
}

void System::calculate_complexQLM_6(){

    //nn = number of neighbors
    int nn;
    double realti,imgti;
    double realYLM,imgYLM;

    // nop = parameter.nop;
    for (int ti= 0;ti<nop;ti++){

        nn = atoms[ti].n_neighbors;
        for (int mi = -6;mi < 7;mi++){

            realti = 0.0;
            imgti = 0.0;
            for (int ci = 0;ci<nn;ci++){

                QLM(6,mi,atoms[ti].n_theta[ci],atoms[ti].n_phi[ci],realYLM, imgYLM);
                realti += atoms[ti].neighborweight[ci]*realYLM;
                imgti += atoms[ti].neighborweight[ci]*imgYLM;
            }

            realti = realti/(double(nn));
            imgti = imgti/(double(nn));
            atoms[ti].realq[4][mi+6] = realti;
            atoms[ti].imgq[4][mi+6] = imgti;
        }
    }
}

void System::GlobalBOO_Bond(vector <int> atomlist)
{
    vector<double>bondpostemp(3, 0),bondvectemp(3,0);
    int nn,q,ti=0;
    bondpos.clear();
    bondvec.clear();
    bondpos.resize(0);
    bondvec.resize(0);
    for (vector<int>::iterator it = atomlist.begin(); it != atomlist.end(); it++) {
            ti = *it;
            nn = atoms[ti].n_neighbors;
        for (int ci = 0; ci < nn; ci++) {
            if(atoms[ti].neighbors[ci]> ti){//对每对键只算一次
            
                if (atoms[ti].condition != atoms[atoms[ti].neighbors[ci]].condition) continue;
                bondpostemp= vector<double>(3, 0);
                bondvectemp = vector<double>(3, 0);

                bondpostemp[0] = (atoms[ti].posx + atoms[atoms[ti].neighbors[ci]].posx) / 2;
                bondpostemp[1] = (atoms[ti].posy + atoms[atoms[ti].neighbors[ci]].posy) / 2;
                bondpostemp[2] = (atoms[ti].posz + atoms[atoms[ti].neighbors[ci]].posz) / 2;

                bondvectemp[0] = (atoms[ti].posx - atoms[atoms[ti].neighbors[ci]].posx) ;
                bondvectemp[1] = (atoms[ti].posy - atoms[atoms[ti].neighbors[ci]].posy) ;
                bondvectemp[2] = (atoms[ti].posz - atoms[atoms[ti].neighbors[ci]].posz) ;
                bondpos.emplace_back(bondpostemp);
                bondvec.emplace_back(bondvectemp);
            }
        }
    }
 }
 
 
 
 
void  System::GlobalBOO_Sum(vector <int>qs){
    vector<vector<vector<double>>> res;
    complex<double>Qlm1, Qlm2, Qlm3,Complexsum;
    int nn, q;
    double realall = 0, imgall = 0, weightsum = 0, realYLM, imgYLM;
    double itheta = 0, iphi = 0,d=0,summ=0, wig=0;
    global_Qlm.clear();
    global_Qlm.resize(2);
    global_Ql.clear();
    global_Ql.resize(0);
    global_Wl.clear();
    global_Wl.resize(0);
    global_Wlnorm.clear();
    global_Wlnorm.resize(0);
    for (int tq = 0; tq < qs.size(); tq++) {
        global_Qlm[0].emplace_back(vector<double>());
        global_Qlm[1].emplace_back(vector<double>());
        global_Ql.emplace_back(0);
        global_Wl.emplace_back(0);
        global_Wlnorm.emplace_back(0);
        q = qs[tq];
        summ = 0;
        for (int mi = -q; mi < q + 1; mi++) {
            global_Qlm[0][tq].emplace_back(0);
            global_Qlm[1][tq].emplace_back(0);
            for (int i = 0; i < bondvec.size();i++) {
                d = pow(bondvec[i][0] * bondvec[i][0] + bondvec[i][1] * bondvec[i][1] + bondvec[i][2] * bondvec[i][2], 0.5);
                itheta = acos(bondvec[i][2] / d);//acos z/r
                iphi= atan2(bondvec[i][1], bondvec[i][0]);//atan2(y,x)
                QLM(q, mi, itheta, iphi, realYLM, imgYLM);
                //realti += atoms[ti].neighborweight[ci] * realYLM;
                //imgti += atoms[ti].neighborweight[ci] * imgYLM;
                global_Qlm[0][tq][mi + q] += realYLM;
                global_Qlm[1][tq][mi + q] += imgYLM;
                weightsum += 1;
            }
            global_Qlm[0][tq][mi + q] /= float(weightsum);
            global_Qlm[1][tq][mi + q] /= float(weightsum);
            weightsum = 0;
            summ += global_Qlm[0][tq][mi + q] * global_Qlm[0][tq][mi + q] + global_Qlm[1][tq][mi + q] * global_Qlm[1][tq][mi + q];
        }
        summ = pow(((4.0 * PI / (2 * q + 1)) * summ), 0.5);
        global_Ql[tq] = summ;
        Complexsum = 0;
        for (int m1 = -q; m1 < q + 1; m1++) {
            for (int m2 = -q; m2 < q + 1; m2++) {
                for (int m3 = -q; m3 < q + 1; m3++) {
                    if (m1 + m2 + m3 == 0) {
                        wig=WignerSymbols::wigner3j(q,q,q,m1,m2,m3);
                        
                        Qlm1= global_Qlm[0][tq][m1 + q] +global_Qlm[1][tq][m1 + q] *1i;
                        Qlm2= global_Qlm[0][tq][m2 + q] +global_Qlm[1][tq][m2 + q]*1i;
                        Qlm3= global_Qlm[0][tq][m3 + q] +global_Qlm[1][tq][m3 + q]*1i;
                        Complexsum += wig*Qlm1*Qlm2*Qlm3;

                    }
                }
            }
        }
        global_Wl[tq] = Complexsum.real();
        global_Wlnorm[tq] = Complexsum.real() / pow(global_Ql[tq], 3) * pow(4 * PI / (2 * q + 1), 3.0 / 2.0);
    }
}
vector<vector<double>> System::GlobalBOO_CF(vector <int>qs, double cut, int histnum, double histlow,bool norm,int n1,int n2,int n3,bool ffton){
    //Initialation
    complex<double>Qlm_a1, Qlm_a2, Complexsum, G0temp;
    double dx = boxx / n1, dy = boxy / n2, dz = boxz / n3;
    double dV = dx * dy * dz;
    double d, diffx, diffy, diffz;
    int q, ti = 0;
    double realall = 0, imgall = 0, weightsum = 0, realYLM, imgYLM;
    double deltacut = (cut - histlow) / histnum;

    vector<double> G0(histnum,0);
    vector<complex<double>> G0tempQ;
    double y0 = pow((1 / (4 * PI)), 0.5);
    G0tempQ.resize(bondpos.size(), y0);

    vector<vector<double>> res;
    vector<int> rescount(histnum, 0);
    for (int tq = 0; tq < qs.size(); tq++) {
        res.emplace_back(vector<double>(histnum, 0));
    }

    //calculate Qlm for each bond.
    vector<vector<vector<complex<double>>>> tempQ;
    tempQ.resize(bondpos.size());
    for (int a1 = 0; a1 < bondpos.size(); a1++) {
        tempQ[a1].resize(qs.size());
        for (int tq = 0; tq < qs.size(); tq++) {
            q = qs[tq];
            tempQ[a1][tq].resize(2 * q + 1);
            for (int mi = -q; mi < q + 1; mi++) {
                double d1 = pow(bondvec[a1][0] * bondvec[a1][0] + bondvec[a1][1] * bondvec[a1][1] + bondvec[a1][2] * bondvec[a1][2], 0.5);
                double itheta1 = acos(bondvec[a1][2] / d1);//acos z/r
                double iphi1 = atan2(bondvec[a1][1], bondvec[a1][0]);//atan2(y,x)
                QLM(q, mi, itheta1, iphi1, realYLM, imgYLM);
                tempQ[a1][tq][mi+q] = realYLM + imgYLM * 1i;
            }
        }
    }
    if (ffton) {
        complex<double>* fftin = (complex<double>*)fftw_malloc((sizeof(fftw_complex) * n1 * n2 * n3));
        complex<double>* fftout = (complex<double>*)fftw_malloc((sizeof(fftw_complex) * n1 * n2 * n3));
        fftw_plan fft = fftw_plan_dft_3d(n1, n2, n3, (fftw_complex*)fftin, (fftw_complex*)fftout, FFTW_FORWARD, FFTW_ESTIMATE);
        fftw_plan ifft = fftw_plan_dft_3d(n1, n2, n3, (fftw_complex*)fftout, (fftw_complex*)fftin, FFTW_BACKWARD, FFTW_ESTIMATE);
        for (int i = 0; i < n1 * n2 * n3; i++) {
            fftin[i] = 0;
            fftout[i] = 0;
        }

        //calculate the histbin number for each fft grid point.
        vector<vector<vector<int>>> gridtohist;
        gridtohist.resize(n1);
        for (int i = 0; i < n1; i++) {
            gridtohist[i].resize(n2);
            for (int j = 0; j < n2; j++) {
                gridtohist[i][j].resize(n3);
                for (int k = 0; k < n3; k++) {
                    diffx = i * dx;
                    diffy = j * dy;
                    diffz = k * dz;
                    d = sqrt(diffx * diffx + diffy * diffy + diffz * diffz);
                    if (d <= cut && d >= histlow) {
                        gridtohist[i][j][k] = floor((d - histlow) / deltacut);
                    }
                    else {
                        gridtohist[i][j][k] = -1;
                    }
                }
            }
        }

        //use fft and fft-grid to calculate Qlm correlation function
        for (int tq = 0; tq < qs.size(); tq++) {
            q = qs[tq];
            for (int mi = -q; mi < q + 1; mi++) {
                for (int i = 0; i < n1 * n2 * n3; i++) {
                    fftin[i] = 0;
                    fftout[i] = 0;
                }
                for (int a1 = 0; a1 < bondpos.size(); a1++) {
                    int ix = ((int)round(bondpos[a1][0] / dx)) % n1;
                    int iy = ((int)round(bondpos[a1][1] / dy)) % n2;
                    int iz = ((int)round(bondpos[a1][2] / dz)) % n3;
                    if (ix < 0) { ix += n1; }
                    if (iy < 0) { iy += n2; }
                    if (iz < 0) { iz += n3; }
                    fftin[ix * n2 * n3 + iy * n3 + iz] += tempQ[a1][tq][mi + q];
                }
                fftw_execute(fft);
                for (int i = 0; i < n1 * n2 * n3; i++) {
                    fftout[i] = fftout[i] * conj(fftout[i]);
                }
                for (int i = 0; i < n1 * n2 * n3; i++) {
                    fftin[i] = 0;
                    //fftout[i] = 0;
                }
                fftw_execute(ifft);
                for (int i = 0; i < n1 * n2 * n3; i++) {
                    fftin[i] = fftin[i].real() / n1 / n2 / n3;
                }
                for (int i = 0; i < n1; i++) {
                    for (int j = 0; j < n2; j++) {
                        for (int k = 0; k < n3; k++) {

                            if (gridtohist[i][j][k] > 0) {
                                res[tq][gridtohist[i][j][k]] += fftin[i * n2*n3 + j * n3 + k].real();
                            }
                        }
                    }
                }
            }
        }
        //use fft and fft-grid to calculate Q00 correlation function(G0) and also make the count for each histogram bin.
        for (int i = 0; i < n1 * n2 * n3; i++) {
            fftin[i] = 0;
            fftout[i] = 0;
        }
        for (int a1 = 0; a1 < bondpos.size(); a1++) {
            int ix = ((int)round(bondpos[a1][0] / dx)) % n1;
            int iy = ((int)round(bondpos[a1][1] / dy)) % n2;
            int iz = ((int)round(bondpos[a1][2] / dz)) % n3;
            if (ix < 0) { ix += n1; }
            if (iy < 0) { iy += n2; }
            if (iz < 0) { iz += n3; }
            fftin[ix * n2*n3 + iy * n3 + iz] += G0tempQ[a1];
        }
        fftw_execute(fft);
        for (int i = 0; i < n1 * n2 * n3; i++) {
            fftout[i] = fftout[i] * conj(fftout[i]);
        }
        for (int i = 0; i < n1 * n2 * n3; i++) {
            fftin[i] = 0;
            //fftout[i] = 0;
        }
        fftw_execute(ifft);
        for (int i = 0; i < n1 * n2 * n3; i++) {
            fftin[i] = fftin[i].real() / n1 / n2 / n3;
        }
        for (int i = 0; i < n1; i++) {
            for (int j = 0; j < n2; j++) {
                for (int k = 0; k < n3; k++) {
                    if (gridtohist[i][j][k] > 0) {
                        G0[gridtohist[i][j][k]] += fftin[i * n2*n3 + j * n3 + k].real();
                        //G0[gridtohist[i][j][k]] += 1;
                        rescount[gridtohist[i][j][k]]++;
                    }
                }
            }
        }
        //post-process the data
        for (int i = 0; i < histnum; i++) {

            //G0[i] = G0[i];//正常应该再乘dV*4PI/rescount但是和下面的消掉了

            //G0[i] *= 4 * PI;
            for (int j = 0; j < qs.size(); j++)
            {
                q = qs[j];
                //res[j][i] = res[j][i];
                res[j][i] /= (2 * q + 1);
                if (norm)
                {
                    res[j][i] /= G0[i];//res 和G0 的dV*4PI/rescount彼此消掉。

                }
                else {
                    res[j][i] *= dV * 4 * PI / rescount[i];//这个系数对不对我也不知道，可能还跟/ bondpos.size()有关

                    //res[j][i] = G0[i]/rescount[i];
                }
                if (i == 0) {
                    res[j][0] = 0;
                }
            }
        }
        fftw_free(fftin);
        fftw_free(fftout);
        fftw_destroy_plan(fft);
        fftw_destroy_plan(ifft);
        return res;
    }
    else{
        for (int a1 = 0; a1 < bondpos.size(); a1++) {
            for (int a2 = a1; a2 < bondpos.size(); a2++) {
                diffx = bondpos[a1][0] - bondpos[a2][0];
                diffy = bondpos[a1][1] - bondpos[a2][1];
                diffz = bondpos[a1][2] - bondpos[a2][2];
                if (diffx > boxx / 2.0) { diffx -= boxx; };
                if (diffx < -boxx / 2.0) { diffx += boxx; };
                if (diffy > boxy / 2.0) { diffy -= boxy; };
                if (diffy < -boxy / 2.0) { diffy += boxy; };
                if (diffz > boxz / 2.0) { diffz -= boxz; };
                if (diffz < -boxz / 2.0) { diffz += boxz; };
                d = sqrt(diffx * diffx + diffy * diffy + diffz * diffz);
                if (d <= cut && d >= histlow) {
                    for (int tq = 0; tq < qs.size(); tq++) {
                        q = qs[tq];
                        Complexsum = 0;
                        for (int mi = -q; mi < q + 1; mi++) {
                            Qlm_a1 = tempQ[a1][tq][mi + q];
                            Qlm_a2 = tempQ[a2][tq][mi + q];
                            Complexsum += Qlm_a1 * conj(Qlm_a2);
                            
                            //rescount[tq][floor((d - histlow) / deltacut)] += 1;
                        }
                        res[tq][floor((d - histlow) / deltacut)] += Complexsum.real();
                    }
                    {
                        Qlm_a1 = G0tempQ[a1];
                        Qlm_a2 = G0tempQ[a2];
                        G0temp = Qlm_a1 * conj(Qlm_a2);
                        G0[floor((d - histlow) / deltacut)] += G0temp.real();
                        //G0count[floor((d - histlow) / deltacut)] += 1;
                    }
                }
            }
        }
        double r = 0;
        for (int i = 0; i < histnum; i++) {
            r = deltacut * (i + 0.5);
            //G0[i] = G0[i]/ bondpos.size() / deltacut;//正常应该再比4pi.r2 但是和下面的消掉了

            //G0[i] *= 4 * PI;
            for (int j = 0; j < qs.size(); j++)
            {
                q = qs[j];
                //res[j][i] = res[j][i]/ bondpos.size() / deltacut;
                //res[j][i] *= 4 * PI / (2 * q + 1);
                res[j][i] /= (2 * q + 1);
                if (norm)
                {
                    res[j][i] /=  G0[i];//res 和G0 的4pi r2 彼此消掉。

                }
                else {
                    res[j][i] *= 4 * PI;
                    res[j][i] /= (4 * PI * r * r) * bondpos.size() * deltacut;
                }
            }
        }
        return res;
    }

}
void System::calculate_w(vector <int> qs, vector <int> atomlist,bool averageon) {
    int q;
    int ti = 0;
    double wig;
    complex<double> Qlm1,Qlm2,Qlm3, Complexsum;
    if (!averageon) {
        for (vector<int>::iterator it = atomlist.begin(); it != atomlist.end(); it++) {
            ti = *it;
            for (int tq = 0; tq < qs.size(); tq++) {
                q = qs[tq];
                Complexsum = 0;
                for (int m1 = -q; m1 < q + 1; m1++) {
                    for (int m2 = m1; m2 < q + 1; m2++) {
                        int m3 = 0 - m1 - m2;
                        wig = WignerSymbols::wigner3j(q, q, q, m1, m2, m3);
                        Qlm1 = atoms[ti].realq[q - 2][m1 + q] + atoms[ti].imgq[q - 2][m1 + q] * 1i;
                        Qlm2 = atoms[ti].realq[q - 2][m2 + q] + atoms[ti].imgq[q - 2][m2 + q] * 1i;
                        Qlm3 = atoms[ti].realq[q - 2][m3 + q] + atoms[ti].imgq[q - 2][m3 + q] * 1i;
                        if (m2 == m1) {
                        Complexsum += wig * Qlm1 * Qlm2 * Qlm3;
                        }
                        else if (m2 > m1)
                        {
                            Complexsum += 2* wig * Qlm1 * Qlm2 * Qlm3;
                        }

                            
                        
                    }
                }
                atoms[ti].w[q-2] = Complexsum.real();
                atoms[ti].wnorm[q-2] = Complexsum.real() / pow(atoms[ti].q[q-2], 3) * pow(4 * PI / (2 * q + 1), 3.0 / 2.0);
            }
        }
    }
    else {
        for (vector<int>::iterator it = atomlist.begin(); it != atomlist.end(); it++) {
            ti = *it;
            for (int tq = 0; tq < qs.size(); tq++) {
                q = qs[tq];
                Complexsum = 0;
                for (int m1 = -q; m1 < q + 1; m1++) {
                    for (int m2 = m1; m2 < q + 1; m2++) {
                        int m3 = 0 - m1 - m2;
                                wig = WignerSymbols::wigner3j(q, q, q, m1, m2, m3);
                                Qlm1 = atoms[ti].arealq[q - 2][m1 + q] + atoms[ti].aimgq[q - 2][m1 + q] * 1i;
                                Qlm2 = atoms[ti].arealq[q - 2][m2 + q] + atoms[ti].aimgq[q - 2][m2 + q] * 1i;
                                Qlm3 = atoms[ti].arealq[q - 2][m3 + q] + atoms[ti].aimgq[q - 2][m3 + q] * 1i;
                                if (m2 == m1) {
                                    Complexsum += wig * Qlm1 * Qlm2 * Qlm3;
                                }
                                else if (m2 > m1)
                                {
                                    Complexsum += 2 * wig * Qlm1 * Qlm2 * Qlm3;
                                }

                            
                        
                    }
                }
                atoms[ti].aw[q - 2] = Complexsum.real();
                atoms[ti].awnorm[q - 2] = Complexsum.real() / pow(atoms[ti].aq[q - 2], 3) * pow(4 * PI / (2 * q + 1), 3.0 / 2.0);
            }
        }
    }
}

//calculation of any complex qval
void System::calculate_q(vector <int> qs,vector <int> atomlist){

    //set_reqd_qs(qs);

    //nn = number of neighbors
    int nn;
    double realti,imgti;
    double realYLM,imgYLM;
    int q;
    double summ;
    int ti=0;

    //first make space in atoms for the number of qs needed - assign with null values
    /*
    for(int ti=0;ti<nop;ti++){
        for(int tj=0;tj<11;tj++){

            atoms[ti].q[tj] = -1;
            atoms[ti].aq[tj] = -1;
            for(int tk=0;tk<25;tk++){
                atoms[ti].realq[tj][tk] = 0;
                atoms[ti].imgq[tj][tk] = 0;
                atoms[ti].arealq[tj][tk] = 0;
                atoms[ti].aimgq[tj][tk] = 0;
            }
        }
    }
    */

    //note that the qvals will be in -2 pos
    //q2 will be in q0 pos and so on
    double weightsum;
    // nop = parameter.nop;
    //for (int ti= 0;ti<nop;ti++){
    for (vector<int>::iterator it = atomlist.begin();it != atomlist.end();it++){
        ti = *it;
        nn = atoms[ti].n_neighbors;
        //for(int tq=0;tq<lenqs;tq++){
        for(int tq=0;tq<qs.size();tq++){
            //find which q?
            q = qs[tq];
            //cout<<q<<endl;
            summ = 0;
            for (int mi = -q;mi < q+1;mi++){
                realti = 0.0;
                imgti = 0.0;
                weightsum = 0;
                for (int ci = 0;ci<nn;ci++){
                    if (atoms[ti].condition != atoms[atoms[ti].neighbors[ci]].condition) continue;
                    QLM(q,mi,atoms[ti].n_theta[ci],atoms[ti].n_phi[ci],realYLM, imgYLM);
                    realti += atoms[ti].neighborweight[ci]*realYLM;
                    imgti += atoms[ti].neighborweight[ci]*imgYLM;
                    weightsum += atoms[ti].neighborweight[ci];
                }

            //the weights are not normalised,
            if(!voronoiused){
                realti = realti/float(weightsum);
                imgti = imgti/float(weightsum);
            }


            atoms[ti].realq[q-2][mi+q] = realti;
            atoms[ti].imgq[q-2][mi+q] = imgti;

            summ+= realti*realti + imgti*imgti;
            //summ+= realti;
            }
            //normalise summ
            summ = pow(((4.0*PI/(2*q+1)) * summ),0.5);
            atoms[ti].q[q-2] = summ;

        }

    }

}


//calculation of any complex aqvalb
void System::calculate_aq(vector <int> qs,vector <int> atomlist){

    //nn = number of neighbors
    int nn;
    double realti,imgti;
    //double realYLM,imgYLM;
    int q;
    double summ;
    int nns;
    int ti=0;

    //if (!qsfound) { calculate_q(qs); }
    //note that the qvals will be in -2 pos
    //q2 will be in q0 pos and so on

    // nop = parameter.nop;
    //for (int ti= 0;ti<nop;ti++){
    for (vector<int>::iterator it = atomlist.begin();it != atomlist.end();it++){
        ti = *it;

        nn = atoms[ti].n_neighbors;

        for(int tq=0;tq<qs.size();tq++){
            //find which q?
            q = qs[tq];
            //cout<<q<<endl;
            summ = 0;
            for (int mi = 0;mi < 2*q+1;mi++){
                realti = atoms[ti].realq[q-2][mi];
                imgti = atoms[ti].imgq[q-2][mi];
                nns = 0;
                for (int ci = 0;ci<nn;ci++){
                    if (atoms[ti].condition != atoms[atoms[ti].neighbors[ci]].condition) continue; 
                    realti += atoms[atoms[ti].neighbors[ci]].realq[q-2][mi];
                    imgti += atoms[atoms[ti].neighbors[ci]].imgq[q-2][mi];
                    nns += 1;
                }

            //realti = realti/weightsum;
            //imgti = realti/weightsum;

            realti = realti/(double(nns+1));
            imgti = imgti/(double(nns+1));

            atoms[ti].arealq[q-2][mi] = realti;
            atoms[ti].aimgq[q-2][mi] = imgti;

            summ+= realti*realti + imgti*imgti;
            }
            //normalise summ
            summ = pow(((4.0*PI/(2*q+1)) * summ),0.5);
            atoms[ti].aq[q-2] = summ;

        }

    }
}

vector<double> System::gqvals(int qq){
    vector<double> qres;
    qres.reserve(real_nop);
    for(int i=0;i<real_nop;i++){
        qres.emplace_back(atoms[i].q[qq-2]);
    }

    return qres;
}

vector<double> System::gaqvals(int qq){
    vector<double> qres;
    qres.reserve(real_nop);
    for(int i=0;i<real_nop;i++){
        qres.emplace_back(atoms[i].aq[qq-2]);
    }

    return qres;
}

void System::calculate_disorder(){

    //for disorder we need sjj which is dot product with itself, self dot prouct of neighbors
    //and cross dot product
    double sumSquareti,sumSquaretj;
    double realdotproduct,imgdotproduct;
    double connection;
    double dis;

    for(int ti=0; ti<nop; ti++){

        sumSquareti = 0.0;
        realdotproduct = 0.0;
        imgdotproduct = 0.0;

        for (int mi = 0;mi < 2*solidq+1 ;mi++){
            sumSquareti += atoms[ti].realq[solidq-2][mi]*atoms[ti].realq[solidq-2][mi] + atoms[ti].imgq[solidq-2][mi] *atoms[ti].imgq[solidq-2][mi];
            realdotproduct += atoms[ti].realq[solidq-2][mi]*atoms[ti].realq[solidq-2][mi];
            imgdotproduct  += atoms[ti].imgq[solidq-2][mi] *atoms[ti].imgq[solidq-2][mi];
        }
        connection = (realdotproduct+imgdotproduct)/(sqrt(sumSquareti)*sqrt(sumSquareti));
        atoms[ti].sii = connection;

    }

    //first round is over
    //now find cross terms
    for(int ti=0; ti<nop; ti++){

        sumSquareti = 0.0;
        sumSquaretj = 0.0;
        realdotproduct = 0.0;
        imgdotproduct = 0.0;
        dis = 0;

        for(int tj=0; tj<atoms[ti].n_neighbors; tj++){
            for (int mi = 0;mi < 2*solidq+1 ;mi++){
                sumSquareti += atoms[ti].realq[solidq-2][mi]*atoms[ti].realq[solidq-2][mi] + atoms[ti].imgq[solidq-2][mi] *atoms[ti].imgq[solidq-2][mi];
                sumSquaretj += atoms[tj].realq[solidq-2][mi]*atoms[tj].realq[solidq-2][mi] + atoms[tj].imgq[solidq-2][mi] *atoms[tj].imgq[solidq-2][mi];
                realdotproduct += atoms[ti].realq[solidq-2][mi]*atoms[tj].realq[solidq-2][mi];
                imgdotproduct  += atoms[ti].imgq[solidq-2][mi] *atoms[tj].imgq[solidq-2][mi];
            }
            connection = (realdotproduct+imgdotproduct)/(sqrt(sumSquaretj)*sqrt(sumSquareti));
            dis += (atoms[ti].sii + atoms[tj].sii - 2*connection);
        }
        atoms[ti].disorder = dis/float(atoms[ti].n_neighbors);

    }
}

void System::find_average_disorder(){
    double vv;
    int nn;

    for (int ti= 0;ti<nop;ti++){
        nn = atoms[ti].n_neighbors;
        vv = atoms[ti].disorder;
        for (int ci = 0; ci<nn; ci++){
            vv += atoms[atoms[ti].neighbors[ci]].disorder;
        }
        vv = vv/(double(nn+1));
        atoms[ti].avgdisorder = vv;
    }
}
//-----------------------------------------------------
// Solids and Clustering methods
//-----------------------------------------------------
//also has to be overloaded - could be a useful function
double System::get_number_from_bond(int ti,int tj){

    double sumSquareti,sumSquaretj;
    double realdotproduct,imgdotproduct;
    double connection;
    sumSquareti = 0.0;
    sumSquaretj = 0.0;
    realdotproduct = 0.0;
    imgdotproduct = 0.0;

    for (int mi = 0;mi < 2*solidq+1 ;mi++){

        sumSquareti += atoms[ti].realq[solidq-2][mi]*atoms[ti].realq[solidq-2][mi] + atoms[ti].imgq[solidq-2][mi] *atoms[ti].imgq[solidq-2][mi];
        sumSquaretj += atoms[tj].realq[solidq-2][mi]*atoms[tj].realq[solidq-2][mi] + atoms[tj].imgq[solidq-2][mi] *atoms[tj].imgq[solidq-2][mi];
        realdotproduct += atoms[ti].realq[solidq-2][mi]*atoms[tj].realq[solidq-2][mi];
        imgdotproduct  += atoms[ti].imgq[solidq-2][mi] *atoms[tj].imgq[solidq-2][mi];
    }

    connection = (realdotproduct+imgdotproduct)/(sqrt(sumSquaretj)*sqrt(sumSquareti));
    //cout<<connection<<endl;
    return connection;
}

//overloaded version
double System::get_number_from_bond(Atom atom1,Atom atom2){

    double sumSquareti,sumSquaretj;
    double realdotproduct,imgdotproduct;
    double connection;
    sumSquareti = 0.0;
    sumSquaretj = 0.0;
    realdotproduct = 0.0;
    imgdotproduct = 0.0;

    for (int mi = 0;mi < 2*solidq+1 ;mi++){

        sumSquareti += atom1.realq[solidq-2][mi]*atom1.realq[solidq-2][mi] + atom1.imgq[solidq-2][mi] *atom1.imgq[solidq-2][mi];
        sumSquaretj += atom2.realq[solidq-2][mi]*atom2.realq[solidq-2][mi] + atom2.imgq[solidq-2][mi] *atom2.imgq[solidq-2][mi];
        realdotproduct += atom1.realq[solidq-2][mi]*atom2.realq[solidq-2][mi];
        imgdotproduct  += atom1.imgq[solidq-2][mi] *atom2.imgq[solidq-2][mi];
    }

    connection = (realdotproduct+imgdotproduct)/(sqrt(sumSquaretj)*sqrt(sumSquareti));
    return connection;
}

void System::calculate_frenkel_numbers(){

    int frenkelcons;
    double scalar;

    for (int ti= 0;ti<nop;ti++){

        frenkelcons = 0;
        atoms[ti].avq6q6 = 0.0;
        for (int c = 0;c<atoms[ti].n_neighbors;c++){

            scalar = get_number_from_bond(ti,atoms[ti].neighbors[c]);
            atoms[ti].sij[c] = scalar;
            if (comparecriteria == 0)
                if (scalar > threshold) frenkelcons += 1;
            else
                if (scalar < threshold) frenkelcons += 1;
            
            atoms[ti].avq6q6 += scalar;
        }

        atoms[ti].frenkelnumber = frenkelcons;
        atoms[ti].avq6q6 /= atoms[ti].n_neighbors;

    }
}


void System::find_solid_atoms(){

    int tfrac;
    if (criteria == 0){
        for (int ti= 0;ti<nop;ti++){
          if (comparecriteria == 0)
            atoms[ti].issolid = ( (atoms[ti].frenkelnumber > minfrenkel) && (atoms[ti].avq6q6 > avgthreshold) );
          else
            atoms[ti].issolid = ( (atoms[ti].frenkelnumber > minfrenkel) && (atoms[ti].avq6q6 < avgthreshold) );
        }
    }
    else if (criteria == 1){
        for (int ti= 0;ti<nop;ti++){
            tfrac = ((atoms[ti].frenkelnumber/double(atoms[ti].n_neighbors)) > minfrenkel);
            if (comparecriteria == 0)
                atoms[ti].issolid = (tfrac && (atoms[ti].avq6q6 > avgthreshold));
            else
                atoms[ti].issolid = (tfrac && (atoms[ti].avq6q6 < avgthreshold));
        }
    }

}


void System::find_clusters(double clustercutoff){
        //Clustering methods should only run over real atoms
        if (clustercutoff != 0){
          for(int ti=0; ti<real_nop;ti++){
              atoms[ti].cutoff = clustercutoff;
          }
        }
        for(int ti=0; ti<real_nop;ti++){
            atoms[ti].belongsto = -1;
        }

        for (int ti= 0;ti<real_nop;ti++){

            if (!atoms[ti].condition) continue;
            if (atoms[ti].ghost) continue;

            if (atoms[ti].belongsto==-1) {atoms[ti].belongsto = atoms[ti].id; }
            for (int c = 0;c<atoms[ti].n_neighbors;c++){

                if(!atoms[atoms[ti].neighbors[c]].condition) continue;
                if(!(atoms[ti].neighbordist[atoms[ti].neighbors[c]] <= atoms[ti].cutoff)) continue;
                if (atoms[atoms[ti].neighbors[c]].ghost) continue;
                if (atoms[atoms[ti].neighbors[c]].belongsto==-1){
                    atoms[atoms[ti].neighbors[c]].belongsto = atoms[ti].belongsto;
                }
                else{
                    atoms[ti].belongsto = atoms[atoms[ti].neighbors[c]].belongsto;
                }
            }
        }
}

//we have to test with a recursive algorithm - to match the values that is presented
//in Grisells code.
void System::harvest_cluster(const int ti, const int clusterindex){

    int neigh;
    for(int i=0; i<atoms[ti].n_neighbors; i++){
        neigh = atoms[ti].neighbors[i];
        if (atoms[neigh].ghost) continue;
        if(!atoms[neigh].condition) continue;
        if(!(atoms[ti].neighbordist[i] <= atoms[ti].cutoff)) continue;
        if (atoms[neigh].belongsto==-1){
            atoms[neigh].belongsto = clusterindex;
            harvest_cluster(neigh, clusterindex);
        }
    }
}

void System::find_clusters_recursive(double clustercutoff){

  if (clustercutoff != 0){
    for(int ti=0; ti<nop;ti++){
        atoms[ti].cutoff = clustercutoff;
    }
  }

    int clusterindex;
    clusterindex = 0;

    //reset belongsto indices
    for(int ti=0; ti<real_nop;ti++){
        atoms[ti].belongsto = -1;
    }

    for (int ti= 0;ti<real_nop;ti++){
        if (!atoms[ti].condition) continue;
        if (atoms[ti].ghost) continue;
        if (atoms[ti].belongsto==-1){
            clusterindex += 1;
            atoms[ti].belongsto = clusterindex;
            harvest_cluster(ti, clusterindex);
        }

    }
}



int System::largest_cluster(){

        int *freq = new int[nop];
        for(int ti=0;ti<real_nop;ti++){
            freq[ti] = 0;
        }

        for (int ti= 0;ti<real_nop;ti++)
        {
            if (atoms[ti].belongsto==-1) continue;
            freq[atoms[ti].belongsto-1]++;
        }

        int max=0;
        for (int ti= 0;ti<real_nop;ti++)
        {
            if (freq[ti]>max){
                max=freq[ti];
                maxclusterid = ti+1;
            }

        }

        get_largest_cluster_atoms();

        return max;
}

void System::get_largest_cluster_atoms(){
        for(int ti=0; ti<real_nop; ti++){
            atoms[ti].issurface = 1;
            atoms[ti].lcluster = 0;
            //if its in same cluster as max cluster assign it as one
            if(atoms[ti].belongsto == maxclusterid){
                atoms[ti].lcluster = 1;
            }
           //if its solid- identfy if it has liquid
            if(atoms[ti].issolid == 1){
                atoms[ti].issurface = 0;
                for(int tj=0; tj<atoms[ti].n_neighbors; tj++){
                    if (atoms[atoms[ti].neighbors[tj]].ghost) continue;
                    if(atoms[atoms[ti].neighbors[tj]].issolid == 0){
                        atoms[ti].issurface = 1;
                        break;
                    }
                }
            }
        }
}

void System::set_nucsize_parameters(double n1, double n2, double n3 ) { minfrenkel = n1; threshold = n2; avgthreshold = n3; }

//-----------------------------------------------------
// Voronoi based methods
//-----------------------------------------------------
void System::set_face_cutoff(double fcut){
    face_cutoff = fcut;
}

//overloaded function; would be called
//if neighbor method voronoi is selected.
void System::get_all_neighbors_voronoi(){

    //reset voronoi flag
    voronoiused = 1;

    double d;
    double diffx,diffy,diffz;
    double r,theta,phi;
    int i;
    int ti,id,tnx,tny,tnz, nverts;

    double rx,ry,rz,tsum, fa, x=0, y=0, z=0, vol;//原作者没有初始化xyz ，我看了一下代码将之初始化为0
    
    vector<int> neigh,f_vert, vert_nos;
    vector<double> facearea, v, faceperimeters;
    voronoicell_neighbor c;
    vector< vector<double> > nweights;
    vector< vector<int> > nneighs;
    vector<int> idss;
    //vector<int> nvector;
    double weightsum;
    vector <double> pos;

    //pre_container pcon(boxdims[0][0],boxdims[1][1],boxdims[1][0],boxdims[1][1],boxdims[2][0],boxdims[2][1],true,true,true);
    pre_container pcon(0.00, boxx, 0.00, boxy, 0.0, boxz, true, true, true);
    for(int i=0; i<nop; i++){
        pos = atoms[i].gx();
        pos = remap_atom(pos);
        pcon.put(i, pos[0], pos[1], pos[2]);
    }
    pcon.guess_optimal(tnx,tny,tnz);
    //container con(boxdims[0][0],boxdims[1][1],boxdims[1][0],boxdims[1][1],boxdims[2][0],boxdims[2][1],tnx,tny,tnz,true,true,true, nop);
    container con(0.00, boxx, 0.00, boxy, 0.0, boxz, tnx, tny, tnz, true, true, true, nop);
    pcon.setup(con);

    c_loop_all cl(con);
    if (cl.start()) do if(con.compute_cell(c,cl)) {
            ti=cl.pid();
            c.face_areas(facearea);
            c.neighbors(neigh);
            c.face_orders(f_vert);
            c.face_vertices(vert_nos);
            c.vertices(x,y,z,v);
            c.face_perimeters(faceperimeters);

            vol = c.volume();
            tsum = 0;
            vector <double> dummyweights;
            vector <int> dummyneighs;

            //only loop over neighbors
            weightsum = 0.0;
            for (int i=0; i<facearea.size(); i++){
                weightsum += pow(facearea[i], alpha);
            }


            //assign to nvector
            atoms[ti].volume = vol;
            atoms[ti].vertex_vectors = v;
            atoms[ti].vertex_numbers = vert_nos;
            atoms[ti].cutoff = cbrt(3*vol/(4*3.141592653589793));
            
            //clean up and add vertex positions
            nverts = int(v.size())/3;
            pos = atoms[ti].gx();
            for(int si=0; si<nverts; si++){
                vector<double> temp;
                int li=0;
                for(int vi=si*3; vi<(si*3+3); vi++){
                    temp.emplace_back(v[vi]+pos[li]);
                    li++;
                }
                atoms[ti].vertex_positions.emplace_back(temp);
            }

            //assign to the atom
            //atoms[ti].vorovector = nvector;

            //only loop over neighbors
            //weightsum = 0.0;
            //for (int i=0; i<facearea.size(); i++){
            //    weightsum += facearea[i];
            //}
            for (int tj=0; tj<neigh.size(); tj++){

                //if filter doesnt work continue
                if ((filter == 1) && (atoms[ti].type != atoms[tj].type)){
                    continue;
                }
                else if ((filter == 2) && (atoms[ti].type == atoms[tj].type)){
                    continue;
                }
                atoms[ti].neighbors[tj] = neigh[tj];
                atoms[ti].n_neighbors += 1;
                d = get_abs_distance(ti,neigh[tj],diffx,diffy,diffz);
                atoms[ti].neighbordist[tj] = d;
                //weight is set to 1.0, unless manually reset
                atoms[ti].neighborweight[tj] = pow(facearea[tj], alpha)/weightsum;
                atoms[ti].facevertices[tj] = f_vert[tj];
                atoms[ti].faceperimeters[tj] = faceperimeters[tj];
                atoms[ti].n_diffx[tj] = diffx;
                atoms[ti].n_diffy[tj] = diffy;
                atoms[ti].n_diffz[tj] = diffz;
                convert_to_spherical_coordinates(diffx, diffy, diffz, r, phi, theta);
                atoms[ti].n_r[tj] = r;
                atoms[ti].n_phi[tj] = phi;
                atoms[ti].n_theta[tj] = theta;

            }

    } while (cl.inc());


    //now calculate the averged volume
    find_average_volume();


}


void System::find_average_volume(){
    double vv;
    int nn;

    for (int ti= 0;ti<nop;ti++){
        nn = atoms[ti].n_neighbors;
        vv = atoms[ti].volume;
        for (int ci = 0;ci<nn;ci++){
            vv += atoms[atoms[ti].neighbors[ci]].volume;
        }
        vv = vv/(double(nn+1));
        atoms[ti].avgvolume = vv;
    }
}

//-------------------------------------------------------
// CNA parameters
//-------------------------------------------------------
void System::get_diamond_neighbors(){
    /*
    Get the neighbors in diamond lattice which is part of the
    underlying fcc cell.

    Also store the first and second nearest neighbors for
    latest identification.
    */
    reset_main_neighbors();
    for (int ti=0; ti<nop; ti++){
        //cout<<"ti = "<<ti<<endl;
        //start loop
        for(int j=0 ; j<4; j++){
            int tj = atoms[ti].temp_neighbors[j].index;
            //cout<<"tj = "<<tj<<endl;
            //loop over the neighbors
            atoms[ti].nn1[j] = tj;
            for(int k=0 ; k<4; k++){
                int tk = atoms[tj].temp_neighbors[k].index;
                //cout<<"tk = "<<tk<<endl;
                //now make sure its not the same atom
                if (ti == tk) continue;
                //process the neighbors
                process_neighbor(ti, tk);
            }
        }
    }
}

void System::get_cna_neighbors(int style){
    /*
    Get neighbors for CNA method
    There are two styles available:
    
    Style 1: For FCC like structures (HCP/ICO)
    Style 2: For BCC structure    
    */
    int finished = 1;
    reset_main_neighbors();
    double factor, dist;
    int ncount;

    if (style == 1){
        factor = 0.854;
        ncount = 12;
    }
    else if (style == 2){
        factor = 1.207;
        ncount = 14;
    }

    for (int ti=0; ti<nop; ti++){
        atoms[ti].cutoff = factor*lattice_constant;
        for(int i=0 ; i<ncount; i++){
            int tj = atoms[ti].temp_neighbors[i].index;
            //dist = atoms[ti].temp_neighbors[i].dist;
            //if (dist <= atoms[ti].cutoff)
            process_neighbor(ti, tj);
        }
    }
}

void System::get_acna_neighbors(int style){
    /*
    A new neighbor algorithm that finds a specified number of 
    neighbors for each atom.
    There are two styles available:
    
    Style 1: For FCC like structures (HCP/ICO)
    Style 2: For BCC structure
    */

    double dist;

    //reset neighbors
    reset_main_neighbors();

    if (style == 1){ 
        for (int ti=0; ti<nop; ti++){
            if (atoms[ti].temp_neighbors.size() > 11){
                double ssum = 0;
                for(int i=0 ; i<12; i++){
                    ssum += atoms[ti].temp_neighbors[i].dist;
                }
                //process sum
                atoms[ti].cutoff = 1.207*ssum/12.00;
                //now assign neighbors based on this
                for(int i=0 ; i<12; i++){
                    int tj = atoms[ti].temp_neighbors[i].index;
                    dist = atoms[ti].temp_neighbors[i].dist;
                    //if (dist <= atoms[ti].cutoff)
                    process_neighbor(ti, tj);
                }                                 
            }
        }
    }
    else if (style == 2){
        for (int ti=0; ti<nop; ti++){
            if (atoms[ti].temp_neighbors.size() > 13){
                double ssum = 0;
                for(int i=0 ; i<8; i++){
                    ssum += 1.1547*atoms[ti].temp_neighbors[i].dist;
                }
                for(int i=8 ; i<14; i++){
                    ssum += atoms[ti].temp_neighbors[i].dist;
                }
                atoms[ti].cutoff = 1.207*ssum/14.00;
                //now assign neighbors based on this
                for(int i=0 ; i<14; i++){
                    int tj = atoms[ti].temp_neighbors[i].index;
                    dist = atoms[ti].temp_neighbors[i].dist;
                    //if (dist <= atoms[ti].cutoff)
                    process_neighbor(ti, tj);
                }                                 
            }
        }
    }
}

void System::get_common_neighbors(int ti){
    /*
    Get common neighbors between an atom and its neighbors
    */
    int m, n;
    double d, dx, dy, dz;

    //we have to rest a couple of things first
    //cna vector
    //also common array
    atoms[ti].cna.clear();
    atoms[ti].cna.resize(atoms[ti].n_neighbors);
    atoms[ti].common.clear();
    atoms[ti].common.resize(atoms[ti].n_neighbors);

    for(int i=0; i<atoms[ti].n_neighbors; i++){
        for(int j=0; j<4; j++){
            atoms[ti].cna[i].emplace_back(0);
        }
    }
    
    //now start loop
    for(int i=0; i<atoms[ti].n_neighbors-1; i++){
        m = atoms[ti].neighbors[i];
        for(int j=i+1; j<atoms[ti].n_neighbors; j++){
            n = atoms[ti].neighbors[j];
            d = get_abs_distance(m, n, dx, dy, dz);
            if (d <= atoms[ti].cutoff){
                atoms[ti].cna[i][0]++;
                atoms[ti].common[i].emplace_back(n);
                atoms[ti].cna[j][0]++;
                atoms[ti].common[j].emplace_back(m);
            }
        }
    }
}


void System::get_common_bonds(int ti){
    /*
    Last two steps for CNA analysis
    */
    int c1, c2, maxbonds, minbonds;
    double d, dx, dy, dz;

    //first we clear the bonds array
    atoms[ti].bonds.clear();
    atoms[ti].bonds.resize(atoms[ti].n_neighbors);

    //start loop
    for(int k=0; k<atoms[ti].n_neighbors; k++){
        //clear bonds first
        for(int l=0; l<atoms[ti].cna[k][0]; l++){
            atoms[ti].bonds[k].emplace_back(0);
        }
        //now start proper loop
        for(int l=0; l<atoms[ti].cna[k][0]-1; l++){
            for(int m=l+1; m<atoms[ti].cna[k][0]; m++){
                c1 = atoms[ti].common[k][l];
                c2 = atoms[ti].common[k][m];
                d = get_abs_distance(c1, c2, dx, dy, dz);
                if(d <= atoms[ti].cutoff){
                    atoms[ti].cna[k][1]++;
                    atoms[ti].bonds[k][l]++;
                    atoms[ti].bonds[k][m]++;
                }
            }
        }
        maxbonds = 0;
        minbonds = 8;
        for(int l=0; l<atoms[ti].cna[k][0]; l++){
            maxbonds = max(atoms[ti].bonds[k][l], maxbonds);
            minbonds = min(atoms[ti].bonds[k][l], minbonds);
        }
        atoms[ti].cna[k][2] = maxbonds;
        atoms[ti].cna[k][3] = minbonds;    
    }
}

void System::identify_cn12(){

    int c1, c2, c3, c4;
    int nfcc, nhcp, nico;

    //now we start
    for(int ti=0; ti<nop; ti++){
        if(atoms[ti].structure==0){
            get_common_neighbors(ti);
            get_common_bonds(ti);

            //now assign structure if possible
            nfcc = 0;
            nhcp = 0;
            nico = 0;
            for(int k=0; k<atoms[ti].n_neighbors; k++){
                c1 = atoms[ti].cna[k][0];
                c2 = atoms[ti].cna[k][1];
                c3 = atoms[ti].cna[k][2];
                c4 = atoms[ti].cna[k][3];

                if((c1==4) && (c2==2) && (c3==1) && (c4==1)){
                    nfcc++;
                }
                else if ((c1==4) && (c2==2) && (c3==2) && (c4==0)){
                    nhcp++;
                }
                else if ((c1==5) && (c2==5) && (c3==2) && (c4==2)){
                    nico++;
                }

            }
            if(nfcc==12){
                atoms[ti].structure = 1;
            }
            else if((nfcc==6) && (nhcp==6)){
                atoms[ti].structure = 2;   
            }
            else if (nico==12){
                atoms[ti].structure = 4;   
            }
        }
    }
}

void System::identify_cn14(){
    
    int c1, c2, c3, c4;
    int nbcc1, nbcc2;

    for(int ti=0; ti<nop; ti++){
        if(atoms[ti].structure==0){
            get_common_neighbors(ti);
            get_common_bonds(ti);

            //now assign structure if possible
            nbcc1 = 0;
            nbcc2 = 0;
            for(int k=0; k<atoms[ti].n_neighbors; k++){
                c1 = atoms[ti].cna[k][0];
                c2 = atoms[ti].cna[k][1];
                c3 = atoms[ti].cna[k][2];
                c4 = atoms[ti].cna[k][3];

                if((c1==4) && (c2==4) && (c3==2) && (c4==2)){
                    nbcc1++;
                }
                else if ((c1==6) && (c2==6) && (c3==2) && (c4==2)){
                    nbcc2++;
                }
            }
            if((nbcc1==6) && (nbcc2==8)){
                atoms[ti].structure = 3;   
            }
        }
    }    
}

vector<int> System::identify_diamond_structure(){
    /*
    Calculate diamond structure

    Assign structure numbers
    ------------------------
    5 : Cubic diamond (CD)
    6 : 1NN of CD
    7 : 2NN of CD
    8 : Hexagonal diamond (HD)
    9 : 1NN of HD
    10: 2NN of HD
    */
    //first get lump neighbors
    vector<int> analyis;
    for(int i=0; i<11; i++){
        analyis.emplace_back(0);
    }


    for(int i=0; i<nop; i++){
        atoms[i].structure = 0;
    }

    identify_cndia();
    //gather results
    for(int ti=0; ti<real_nop; ti++){
        analyis[atoms[ti].structure] += 1;
    }

    return analyis;

}

void System::identify_cndia(){
    /*
    Identify diamond structure

    Assign structure numbers
    ------------------------
    5 : Cubic diamond (CD)
    6 : 1NN of CD
    7 : 2NN of CD
    8 : Hexagonal diamond (HD)
    9 : 1NN of HD
    10: 2NN of HD
    */
    //now get diamond neighbors
    get_diamond_neighbors();

    //calculate cutoffs
    for (int ti=0; ti<nop; ti++){
        if (atoms[ti].n_neighbors > 11){
            double ssum = 0;
            for(int i=0 ; i<12; i++){
                ssum += atoms[ti].neighbordist[i];
            }
            //process sum
            atoms[ti].cutoff = 1.207*ssum/12.00;
            //now assign neighbors based on this
        }
    }

    //now calculate cna signature for each atom and assign
    //structures - but only check 12 signature
    identify_cn12();
    int n;
    //now for each atom
    for(int ti=0; ti<nop; ti++){
        if(atoms[ti].structure == 1){
            atoms[ti].structure = 5;
        }
        else if(atoms[ti].structure == 2){
            atoms[ti].structure = 8;
        }
    }
    //second pass
    for(int ti=0; ti<nop; ti++){
        if ((atoms[ti].structure != 5) && (atoms[ti].structure != 8)){
            for(int i=0; i<4; i++){
                n = atoms[ti].nn1[i];
                if(atoms[n].structure == 5){
                    atoms[ti].structure = 6;
                    break;
                }
                else if (atoms[n].structure == 8){
                    atoms[ti].structure = 9;
                    break;
                }
            }
        }
    }

    for(int ti=0; ti<nop; ti++){
        if ((atoms[ti].structure != 5) && (atoms[ti].structure != 8) && (atoms[ti].structure != 6) && (atoms[ti].structure != 9)){
            for(int i=0; i<atoms[ti].n_neighbors; i++){
                n = atoms[ti].neighbors[i];
                if(atoms[n].structure == 5){
                    atoms[ti].structure = 7;
                    break;
                }
                else if (atoms[n].structure == 8){
                    atoms[ti].structure = 10;
                    break;
                }
            }
        }
    }
}

vector<int> System::calculate_cna(int method){
    /*
    Calculate CNA or ACNA
    
    Args
    ----
    method : 1 if CNA
             2 if ACNA
    */

    //create array for result
    vector<int> analyis;
    for(int i=0; i<5; i++){
        analyis.emplace_back(0);
    }

    //assign structures to 0
    for(int i=0; i<nop; i++){
        atoms[i].structure = 0;
    }
    
    //first get lump neighbors
    //neighbor method is same
    get_all_neighbors_bynumber(3, 14, 0);

    //first we start by checking for 12 CN 
    //CNA method
    if(method==1){
        get_cna_neighbors(1);
    }
    //ACNA method
    else if (method==2){
        get_acna_neighbors(1);
    }

    //call here
    identify_cn12();

    //now we start by checking for 14 CN 
    //CNA method
    if(method==1){
        get_cna_neighbors(2);
    }
    //ACNA method
    else if (method==2){
        get_acna_neighbors(2);
    }

    //call here
    identify_cn14();

    //gather results
    for(int ti=0; ti<real_nop; ti++){
        analyis[atoms[ti].structure] += 1;
    }

    return analyis;
}
//-------------------------------------------------------
// Other order parameters
//-------------------------------------------------------

//Methods for entropy
double System::switching_fn(double rij, double ra, int M, int N){

    double num = 1.0 - pow((rij/ra), N);
    double denum = 1.0 - pow((rij/ra), M);
    return num/denum;
}

void System::average_entropy(){
    double entsum;
    for(int i=0; i<nop; i++){
        entsum = atoms[i].entropy;
        for(int j=0; j<atoms[i].n_neighbors; j++){
            entsum += atoms[atoms[i].neighbors[j]].entropy;
        }
        atoms[i].avg_entropy = entsum/(double(atoms[i].n_neighbors + 1));
    }
}

void System::average_entropy_switch(double ra, int M, int N){

    double frij;
    double frijsum = 0.0;
    double entfrijsum = 0.0;

    for(int i=0; i<nop; i++){
        
        frijsum = 0.0;
        entfrijsum = 0.0;

        for(int j=0; j<atoms[i].n_neighbors; j++){
            frij = switching_fn(atoms[i].neighbordist[j], ra, M, N);
            frijsum += frij;
            entfrijsum += atoms[atoms[i].neighbors[j]].entropy*frij;
        }

        atoms[i].avg_entropy = (entfrijsum + atoms[i].entropy)/(frijsum + 1.0);
    }
}


void System::entropy(double sigma, double rho, double rstart, double rstop, double h, double kb){

    for(int i=0; i<nop; i++){
        
        atoms[i].sigma = sigma;
        if (rho == 0){
            rho = atoms[i].n_neighbors/(4.1887902047863905*pow(atoms[i].cutoff,3));
        }
        atoms[i].rho = rho;
        atoms[i].rstart = rstart;
        atoms[i].rstop = rstop;
        atoms[i].h = h;
        atoms[i].kb = kb;

        atoms[i].trapezoid_integration();
    }
}


void System::calculate_centrosymmetry_atom(int ti, int nmax){
    /*
    Calculate centrosymmetry parameter
    */
    //loop over neighbors
    
    double d, dx, dy, dz, weight;
    int t1, t2;
    vector<datom> temp;
    int count = 0;

    for(int i=0; i<atoms[ti].n_neighbors-1; i++){
        for(int j=i+1; j<atoms[ti].n_neighbors; j++){
            //now get dist vectors
            dx = atoms[ti].n_diffx[i] + atoms[ti].n_diffx[j];
            dy = atoms[ti].n_diffy[i] + atoms[ti].n_diffy[j];
            dz = atoms[ti].n_diffz[i] + atoms[ti].n_diffz[j];
            weight = sqrt(dx*dx+dy*dy+dz*dz);
            datom x = {weight, count};
            temp.emplace_back(x);
            count++;            
        }
    }

    //now sort
    sort(temp.begin(), temp.end(), by_dist());

    //now do the second loop
    double csym = 0;

    for(int i=0; i<nmax/2; i++){
        csym += temp[i].dist*temp[i].dist;
    }

    atoms[ti].centrosymmetry = csym;
}

void System::calculate_centrosymmetry(int nmax){

    reset_all_neighbors();
    get_all_neighbors_bynumber(3, nmax, 1);

    //now loop over atoms and call
    for(int ti=0; ti<nop; ti++){
        calculate_centrosymmetry_atom(ti, nmax);
    }
}

vector<double> System::get_centrosymmetry(){

    vector<double> csm;
    for (int i=0; i<real_nop; i++){
        csm.emplace_back(atoms[i].centrosymmetry);
    }
    return csm;
}
