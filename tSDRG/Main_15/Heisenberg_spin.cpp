#include <iostream>
#include <fstream>
#include <sstream>
#include <random>
#include <iomanip>
#include <cmath>
#include <ctime>
#include <algorithm>
#include <uni10.hpp>
#include "../MPO/mpo.h"
#include "../tSDRG_tools/tSDRG_tools.h"
#include "../tSDRG_tools/measure.h"

using namespace std;

void tSDRG_XXZ(int L, int chi, int Pdis, double Jdis, string BC, double S, double Jz, double Dim, double h, int Jseed)
{
    random_device rd;            // non-deterministic generator.
    // mt19937 genRandom(rd() );    // use mersenne twister and seed is rd.
    mt19937 genFixed(Jseed);     // use mersenne twister and seed is fixed!

    uniform_real_distribution<double> Dist_J(nextafter(0.0, 1.0), 1.0); // probability distribution of J rand(0^+ ~ 1)
     
    /// create coupling list and MPO chain for OBC or PBC
    vector<double> J_list;
    if (BC == "PBC")
    {
        for(int i=0; i<L; i++)
        {
            double jvar = Dist_J(genFixed);
            jvar = (1 + Dim * pow(-1,i+1)) * Distribution_Random_Variable(Pdis, jvar, Jdis);
            J_list.push_back(jvar);
        }
    }    
    else if (BC == "OBC")
    {
        for(int i=0; i<L-1; i++)
        {
            double jvar = Dist_J(genFixed);
            jvar = (1 + Dim * pow(-1,i+1)) * Distribution_Random_Variable(Pdis, jvar, Jdis);
            J_list.push_back(jvar);
        }

    }

    /// STEP.1: Decompose the Hamiltonian into MPO blocks
    vector<MPO> MPO_chain;
    MPO_chain = generate_MPO_chain(L, "XXZ_" + BC, S, J_list, Jz, h);

    /// create folder in order to save data
    string file, dis, dim, folder, file_name, file_name1, file_name2, file_nameS;
    if (Jdis < 1.0 && Jdis < 0.1)
        dis = "00" + to_string( (int)(round(Jdis*100)) );
    else if (Jdis < 1.0 && Jdis >= 0.1)
        dis = "0" +  to_string( (int)(round(Jdis*100)) ); 
    else if (Jdis >= 1.0)
        dis = to_string( (int)(round(Jdis*100)) );
    
    if (Dim < 1.0 && Dim < 0.1)
        dim = "00" + to_string( (int)(round(Dim*100)) );
    else if (Dim < 1.0 && Dim >= 0.1)
        dim = "0" + to_string( (int)(round(Dim*100)) );
    else if (Dim >= 1.0)
        dim = to_string( (int)(round(Dim*100)) );

    /// return: TTN(w_up and w_loc) and energy spectrum of top tensor
    vector<uni10::UniTensor<double> > w_up;      // w_up is isometry tensor = VT
    vector<int> w_loc;                           // location of each w
    vector<double> En = J_list;                  // J_list will earse to one, and return ground energy.
    bool info = 1;                               // True; if tSDRG can not find non-zero gap, info return 0, and stop this random seed.
    bool save_RG_info = 0;                       // save gaps at RG stage 
    tSDRG(MPO_chain, En, w_up, w_loc, chi, dis, Pdis, Jseed, save_RG_info, info);

    /// create folder
    //folder = "data/" + BC + "/Jdis" + dis + "/L" + to_string(L) + "_P" + to_string(Pdis) + "_m" + to_string(chi) + "_" + to_string(Jseed);
    folder = "data/" + BC + "/Jdis" + dis + "/Dim" + dim + "/L" + to_string(L) + "_P" + to_string(Pdis) + "_m" + to_string(chi) + "_" + to_string(Jseed);
    string str = "mkdir -p " + folder;
    const char *mkdir = str.c_str();
    const int dir_err = system(mkdir);
    if (dir_err == -1)
    {
        cout << "Error creating directory!" << endl;
        exit(1);
    }

    /// check info if can not RG_J
    if (info == 0)
    {
        cout << "random seed " + to_string(Jseed)  + " died (can not find non-zero gap) " << endl;
        return;
    }
    else
    {
        cout << "finish in " << folder << endl;
    }

    for (int i=0; i<10; i++)
        cout << En[i] << endl;

    //string top1 = Decision_tree(w_loc, true);

    /// create isometry of other part
    vector<uni10::UniTensor<double> > w_down;    // w_down
    uni10::UniTensor<double> kara;
    w_down.assign(L-1, kara);
    for (int i = 0; i < w_up.size(); i++)
    {
        w_down[i] = w_up[i];
        uni10::Permute(w_down[i], {-3, -1, 1}, 2, uni10::INPLACE);
    }

    /// save TTN; Don't do this. Your HDD will be fill with TTN
    /*file = folder;
    for (int i = 0; i < w_up.size(); i++)
    {
        file = folder + "/w_up" + to_string(i);
        w_up[i].Save(file);
    }*/

    /// save disorder coupling J list 
    vector<double> corr12;            // corr <S1><S2>
    file = folder + "/J_list.csv";
    ofstream fout(file);              // == fout.open(file);
    if (!fout)
    {
        ostringstream err;
        err << "Error: Fail to save (maybe need mkdir " << file << ")";
        throw runtime_error(err.str());
    }
    for (int i = 0; i < J_list.size(); i++)
    {
        fout << setprecision(16) << J_list[i] << endl;
        corr12.push_back(Correlation_St(i, w_up, w_down, w_loc) );
    }
    fout.flush();
    fout.close();


    /// save merge order
    file = folder + "/w_loc.csv";
    fout.open(file);
    if (!fout)
    {
        ostringstream err;
        err << "Error: Fail to save (maybe need mkdir " << file << ")";
        throw runtime_error(err.str());
    }
    for (int i = 0; i < w_loc.size(); i++)
    {
        fout << w_loc[i] << endl;
    }
    fout.flush();
    fout.close();

    /// save ground energy
    file = folder + "/energy.csv";
    fout.open(file);
    if (!fout)
    {
        ostringstream err;
        err << "Error: Fail to save (maybe need mkdir " << file << ")";
        throw runtime_error(err.str());
    }
    fout << "energy" << endl;
    for (int i=0; i<En.size(); i++)
    {
        fout << setprecision(16) << En[i] << endl;
    }
    fout.flush();
    fout.close();
    string top1 = Decision_tree(w_loc, false);
    
    /// bulk correlation and string order parameter print
    double corr, corr1 ,corr2, sop;
    file_name1 = folder + "/L" + to_string(L) + "_P" + to_string(Pdis) + "_m" + to_string(chi) + "_" + to_string(Jseed) + "_corr1.csv";
    file_name2 = folder + "/L" + to_string(L) + "_P" + to_string(Pdis) + "_m" + to_string(chi) + "_" + to_string(Jseed) + "_corr2.csv";
    file_nameS = folder + "/L" + to_string(L) + "_P" + to_string(Pdis) + "_m" + to_string(chi) + "_" + to_string(Jseed) + "_string.csv";
    ofstream fout1(file_name1);
    ofstream fout2(file_name2);
    ofstream foutS(file_nameS);
    fout1 << "x1,x2,corr" << endl;
    fout2 << "x1,x2,corr" << endl;
    foutS << "x1,x2,corr" << endl;
    int site1;
    int site2;
    int r;
    /// TODO: for OBC. Now is good for PBC.
    for (site1 = 0; site1 < L-1; site1 += 1)
    {
        int nn;
        if (site1 == 0)
            nn = 1;
        else
            nn = 10;

        for (site2 = site1+nn; site2 < L; site2 += 1)
        {
            r = site2 - site1;
            //cout << r << endl;
            if (r <= L/2)
            {
                //cout << "TEST: " << site1 << " & " << site2 << endl;
                corr = Correlation_StSt(site1, site2, w_up, w_down, w_loc);
                corr1 = corr12[site1];
                corr2 = corr12[site2];

                fout1 << setprecision(16) << site1 << "," << site2 << "," << corr << endl;
                fout2 << setprecision(16) << site1 << "," << site2 << "," << corr - corr1*corr2 << endl;

                if (r == L/2)
                {
                    sop = Correlation_String(site1, site2, w_up, w_down, w_loc);
                    foutS << setprecision(16) << site1 << "," << site2 << "," << sop << endl;
                }
            }
        }
    }
    fout1.flush();
    fout2.flush();
    foutS.flush();
    fout1.close();
    fout2.close();
    foutS.close();
    
    /// save twist order parameter
    file = folder + "/ZL.csv";
    fout.open(file);
    if (!fout)
    {
        ostringstream err;
        err << "Error: Fail to save (maybe need mkdir " << file << ")";
        throw runtime_error(err.str());
    }
    fout << "ZL" << endl;
    double ZL;
    fout << setprecision(16) << (ZL = Correlation_ZL(w_up, w_down, w_loc)) << endl;
    cout << "ZL:" << ZL <<endl;
    fout.flush();
    fout.close();

    //uni10::UniTensor<double> A;
    //uni10::Matrix<double> M;
    //M = w_up[0].GetBlock;
    //A.PutBlock(M);
    /*for (int i=0; i<w_up.size(); i++)
    {
        cout << w_up[i] << endl;
    }*/

    /// Schmidt value = Entanglement spectrum
    /*string top = Decision_tree(w_loc, false);       // find location of top tensor
    vector<double> schmidt_value;
    schmidt_value = Schmidt_Value(w_up.back() );
    
    /// print out
    file_name = "data/Schmidt/" + algo + "/Jdis" + dis + "/L" + to_string(L) + "_P" + to_string(Pdis) + "_m" + to_string(chi) + "_" + to_string(Jseed) + "_value" + top + ".csv";
    fout.open(file_name);
    if (!fout)
    {
        ostringstream err;
        err << "Error: Fail to save (maybe need mkdir " << file_name << ")";
        throw runtime_error(err.str());
    }
    fout << "value" << endl;
    fout.flush();
    for (int i = 0; i < schmidt_value.size(); i++)
    {
        fout << setprecision(16) << schmidt_value[i] << endl;
    }
    fout.flush();
    fout.close();*/
}

void errMsg(char *arg) 
{
    cerr << "Usage: " << arg << " [options]" << endl;
    cerr << "Need 9-parameter:" << endl;
    cerr << "./job.exe <system size> <keep state of RG procedure> <Prob distribution> <disorder> <dimerization> <algo> <seed1> <seed2>\n" << endl;
    cerr << "Example:" << endl;
    cerr << "./job.exe 32 8 10 0.1 0.1 PBC 1 1\n" << endl;
}

int main(int argc, char *argv[])
{
    int L;                      // system size
    int chi;                    // keep state of isometry
    string BC;                  // boundary condition
    int Pdis;                   // model of random variable disturbution
    double Jdis;                // J-coupling disorder strength
    double Dim;			        // Dimerization constant
    int seed1;                  // random seed number in order to repeat data
    int seed2;                  // random seed number in order to repeat data
    double S      = 1.5;        // spin dimension
    double Jz     = 1.0;        // XXZ model
    double h      = 0.0;        // XXZ model

    if (argc == 9)
    {
        stringstream(argv[1]) >> L;
        stringstream(argv[2]) >> chi;
        stringstream(argv[3]) >> Pdis;
        stringstream(argv[4]) >> Jdis;
	    stringstream(argv[5]) >> Dim;
        BC = argv[6];
        stringstream(argv[7]) >> seed1;
        stringstream(argv[8]) >> seed2;
    }
    else
    {
        errMsg(argv[0]);
        return 1;
    }

    for (int Jseed=seed1; Jseed<=seed2; Jseed++)
    {
        tSDRG_XXZ(L, chi, Pdis, Jdis, BC, S, Jz, Dim, h, Jseed);
    }

    return 0;
}