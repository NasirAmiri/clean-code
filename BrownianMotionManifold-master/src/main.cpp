#include <igl/readOFF.h>
#include <igl/per_face_normals.h>
#include <igl/writeOBJ.h>
#include <igl/adjacency_list.h>
#include <igl/unique_edge_map.h>
#include <iostream>
#include <igl/all_edges.h>
#include <igl/edge_flaps.h>
#include <igl/edges.h>
#include <igl/triangle_triangle_adjacency.h>
#include <igl/boundary_facets.h>
#include <igl/grad.h>
#include "Mesh.h"
#include "common.h"
#include "model.h"
#include <omp.h>
#include <cstdlib>

Parameter parameter;
void readParameter(bool flag);
void readParameterExternalForce();
void testModel();
void testMultiPModel();

int main(int argc, char *argv[])
{
    if (argc <= 1) 
    {
        std::cout << "no argument" << std::endl;
        exit(1);
    }
    
    
   
    std::string option = std::string(argv[1]);
    
    std::cout << "option is: " << option << std::endl;
    if (option.compare("SingleP") == 0){
        testModel();
    } else if (option.compare("MultiP") == 0) {
        testMultiPModel();
    } 
    else {
        std::cout << "option unknown! " << option << std::endl;
    }
    
    return 0;
}


void testModel(){
    readParameter(false);
    Model m;
    m.mesh = std::make_shared<Mesh>();
    m.mesh->readMeshFile(parameter.meshFile);
    m.mesh->initialize();
    
    
    for (int c_idx = 0; c_idx < parameter.nCycles; c_idx++){
        m.createInitialState();
        for (int i = 0; i < parameter.numStep; i++){
            m.run();    
        }
    }

}


void testMultiPModel(){
    readParameterExternalForce();
    
    
    Model m;
    m.mesh = std::make_shared<Mesh>();
    m.mesh->readMeshFile(parameter.meshFile);
    m.mesh->initialize();
    
    
    for (int c_idx = 0; c_idx < parameter.nCycles; c_idx++){
        m.createInitialState();
//        m.MCRelaxation();
        for (int i = 0; i < parameter.numStep; i++){
            m.run();    
        }
    }



}


void readParameterExternalForce(){
    std::string line;
    std::ifstream runfile;
    runfile.open("run_multiP.txt");
    getline(runfile, line);
    runfile >> parameter.N;
    getline(runfile, line);
    getline(runfile, line);
    runfile >> parameter.radius;
    getline(runfile, line);
    getline(runfile, line);
    runfile >> parameter.nCycles;
    getline(runfile, line);
    getline(runfile, line);
    runfile >> parameter.numStep;
    getline(runfile, line);
    getline(runfile, line);
    runfile >> parameter.dt;
    getline(runfile, line);
    getline(runfile, line);
    runfile >> parameter.diffu_t;    
    getline(runfile, line);
    getline(runfile, line);
    runfile >> parameter.Bpp;
    getline(runfile, line);
    getline(runfile, line);
    runfile >> parameter.Os_pressure;
    getline(runfile, line);
    getline(runfile, line);
    runfile >> parameter.L_dep;
    getline(runfile, line);
    getline(runfile, line);
    runfile >> parameter.cutoff;   
    getline(runfile, line);
    getline(runfile, line);
    runfile >> parameter.kappa;
    getline(runfile, line);
    getline(runfile, line);
    runfile >> parameter.dip_m;
    getline(runfile, line);
    getline(runfile, line);
    runfile >> parameter.eps_f;
    getline(runfile, line);
    getline(runfile, line);
    runfile >> parameter.eps_s;
    getline(runfile, line);
    getline(runfile, line);
    runfile >> parameter.a_w;
    getline(runfile, line);
    getline(runfile, line);
    runfile >> parameter.eps_z;
    getline(runfile, line);
    getline(runfile, line);
    runfile >> parameter.surft;
    getline(runfile, line);
    getline(runfile, line);
    runfile >> parameter.seed;
    getline(runfile, line);
    getline(runfile, line);
    runfile >> parameter.PDE_dt >> parameter.PDE_nstep;
    getline(runfile, line);
    getline(runfile, line);
    runfile >> parameter.trajOutputInterval;
    getline(runfile, line);
    getline(runfile, line);
    getline(runfile, parameter.iniConfig);
    getline(runfile, line);
    getline(runfile, parameter.filetag);
    getline(runfile, line);
    getline(runfile, parameter.meshFile);
    getline(runfile, line);
    runfile >> parameter.fieldStrength;
    
}

void readParameter( bool multipFlag){
    std::string line;
    std::ifstream runfile;
    runfile.open("run.txt");
    if (!runfile.good()) {
        std::cout << "run.txt file not found!" << std::endl;
        exit(1);
    }
    getline(runfile, line);
    runfile >> parameter.N;
    getline(runfile, line);
    getline(runfile, line);
    runfile >> parameter.radius;
    getline(runfile, line);
    getline(runfile, line);
    runfile >> parameter.nCycles;
    getline(runfile, line);
    getline(runfile, line);
    runfile >> parameter.numStep;
    getline(runfile, line);
    getline(runfile, line);
    runfile >> parameter.dt;
    getline(runfile, line);
    getline(runfile, line);
    runfile >> parameter.diffu_t;    
    getline(runfile, line);
    getline(runfile, line);
    runfile >> parameter.Bpp;
    getline(runfile, line);
    getline(runfile, line);
    runfile >> parameter.Os_pressure;
    getline(runfile, line);
    getline(runfile, line);
    runfile >> parameter.L_dep;
    getline(runfile, line);
    getline(runfile, line);
    runfile >> parameter.cutoff;   
    getline(runfile, line);
    getline(runfile, line);
    runfile >> parameter.kappa;
    getline(runfile, line);
    getline(runfile, line);
    runfile >> parameter.dip_m;
    getline(runfile, line);
    getline(runfile, line);
    runfile >> parameter.eps_f;
    getline(runfile, line);
    getline(runfile, line);
    runfile >> parameter.eps_s;
    getline(runfile, line);
    getline(runfile, line);
    runfile >> parameter.a_w;
    getline(runfile, line);
    getline(runfile, line);
    runfile >> parameter.eps_z;
    getline(runfile, line);
    getline(runfile, line);
    runfile >> parameter.surft;
    getline(runfile, line);
    getline(runfile, line);
    runfile >> parameter.seed;
    getline(runfile, line);
    getline(runfile, line);
    runfile >> parameter.PDE_dt >> parameter.PDE_nstep;
    getline(runfile, line);
    getline(runfile, line);
    runfile >> parameter.trajOutputInterval;
    getline(runfile, line);
    getline(runfile, line);
    getline(runfile, parameter.iniConfig);
    getline(runfile, line);
    getline(runfile, parameter.filetag);
    getline(runfile, line);
    getline(runfile, parameter.meshFile);
    parameter.fieldStrength = 0.0;
    if (multipFlag) {
        getline(runfile, line);
        runfile >> parameter.fieldStrength;
    }
    
}



