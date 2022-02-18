#include "model.h"
#include "common.h"
#include <omp.h>
#include <unordered_map>
double const Model::T = 293.0;
double const Model::kb = 1.38e-23;
double const Model::vis = 1e-3;

extern Parameter parameter;



Model::Model(){
    rand_normal = std::make_shared<std::normal_distribution<double>>(0.0, 1.0);

   
    filetag = parameter.filetag;
    iniFile = parameter.iniConfig;
    numP = parameter.N;
    dimP = parameter.dim;
    radius = parameter.radius;
    dt_ = parameter.dt;
    diffusivity_t = parameter.diffu_t;// this corresponds the diffusivity of 1um particle
    diffusivity_t /= pow(radius,2); // diffusivity will be in unit a^2/s
    Bpp = parameter.Bpp * 1e9 * radius; //2.29 is Bpp/a/kT, now Bpp has unit of kT
    Kappa = parameter.kappa; // here is kappa*radius
    Os_pressure = parameter.Os_pressure * kb * T * 1e9;
    L_dep = parameter.L_dep; // 0.2 of radius size, i.e. 200 nm
    radius_nm = radius*1e9;
    combinedSize = (1+L_dep)*radius_nm;
    mobility = diffusivity_t/kb/T;
    trajOutputInterval = parameter.trajOutputInterval;
    fileCounter = 0;
    cutoff = parameter.cutoff;
    this->rand_generator.seed(parameter.seed);
    srand(parameter.seed);

    for(int i = 0; i < numP; i++){
        particles.push_back(particle_ptr(new Model::particle));
   }
    
}


void Model::run() {
    if (this->timeCounter == 0 || ((this->timeCounter + 1) % trajOutputInterval == 0)) {
        this->outputTrajectory(this->trajOs);
    }

    calForces(); 
    for (int i = 0; i < numP; i++) {            

        for (int j = 0; j < 3; j++){
           particles[i]->vel(j) = diffusivity_t * particles[i]->F(j);
           if (parameter.randomMoveFlag) {
            particles[i]->vel(j) += sqrt(2.0 * diffusivity_t/dt_) *(*rand_normal)(rand_generator);
           }
        }           
        this->moveOnMeshV2(i);
    }
    this->timeCounter++;
    
}

void Model::run(int steps){
    for (int i = 0; i < steps; i++){
	run();
    }
}


void Model::moveOnMeshV2(int p_idx){
    // first calculate the tangent velocity
    Eigen::Vector3d velocity = particles[p_idx]->vel;
    int meshIdx = this->particles[p_idx]->meshFaceIdx;
    Eigen::Vector3d normal = mesh->F_normals.row(meshIdx).transpose();
    Eigen::Vector3d tangentV = velocity - normal*(normal.dot(velocity));
    // local velocity representation
    Eigen::Vector2d localV = mesh->Jacobian_g2l[meshIdx]*tangentV;
    Eigen::Vector2d localQ_new;
    
    double t_residual = this->dt_;
    // the while loop will start with particle lying on the surface
    // need to do wraping do ensure new particle position is finally lying on a surface
    
    if (!mesh->inTriangle(particles[p_idx]->local_r)){
        std::cerr << this->timeCounter << " not in triangle before the loop!" << std::endl;
        std::cout <<  particles[p_idx]->local_r << std::endl;
    }
    double positionPrecision = 1e-8;
    
    while(t_residual > 1e-8){
        // move with local tangent speed
        localQ_new = particles[p_idx]->local_r + localV * t_residual;
        if (mesh->inTriangle(localQ_new)){
            t_residual = 0.0;
            // to avoid tiny negative number
            localQ_new(0) = abs(localQ_new(0));
            localQ_new(1) = abs(localQ_new(1));
            particles[p_idx]->local_r = localQ_new;
            break;
        } else {
            // if localQ_new(0) + localQ_new(1) > 1.0, then must hit edge 1
            // if localQ_new(0) < 0 and localQ_new(1) > 0, then must hit edge 2
            // if localQ_new(0) > 0 and localQ_new(1) < 0, then must hit edge 0
            // if localQ_new(0) < 0 and localQ_new(1) < 0, then we might hit edge 0 and 2
            
            
            // as long as the velocity vector is not parallel to the three edges, there will be a collision
            Eigen::Vector3d t_hit;
            // t_hit will be negative if it move away from the edge
            t_hit(2) = -particles[p_idx]->local_r(0) / localV(0); // this second edge
            t_hit(0) = -particles[p_idx]->local_r(1) / localV(1); // the zero edge
            t_hit(1) = (1 - particles[p_idx]->local_r.sum()) / localV.sum(); // the first edge
            
            double t_min = t_residual;
            int min_idx = -1;
            // the least positive t_hit will hit
            for (int i = 0; i < 3; i++){
                if (t_hit(i) > 1e-12 && t_hit(i) <= t_min){
                    t_min = t_hit(i);
                    min_idx = i;
                }            
            }

            if (min_idx < 0){
                // based on above argument, at least one edge will be hitted
                std::cerr << this->timeCounter << "\t t_hit is not determined!" << std::endl;
                std::cerr << t_hit(0) << "\t" << t_hit(1) << "\t" << t_hit(2) << "\t tmin " << t_min << std::endl;
                std::cout <<  particles[p_idx]->local_r << std::endl;
                std::cout << localQ_new << std::endl;
                std::cout << localV << std::endl;
                
                break;
            }
            
            t_residual -= t_min;

            // here update the local coordinate
            particles[p_idx]->local_r += localV * t_min;
            // here is the correction step to make simulation stable
            if( min_idx == 0){
                // hit the first edge
                particles[p_idx]->local_r(1) = 0.0;                
            } else if( min_idx == 1){
                // hit the second edge, local_r(0) + local_r(1) = 1.0
                particles[p_idx]->local_r(0) = 1.0 - particles[p_idx]->local_r(1);
            } else{
                // hit the third edge
                particles[p_idx]->local_r(0) = 0.0;
            }
            
            int meshIdx = particles[p_idx]->meshFaceIdx;
            int newMeshIdx = mesh->TT(meshIdx, min_idx);


#ifdef DEBUG
            
            int reverseIdx = mesh->TTi(meshIdx,min_idx);
            Eigen::Vector2d newV = mesh->localtransform_v2v[meshIdx][min_idx]*localV;
            Eigen::Vector2d oldV = mesh->localtransform_v2v[mesh->TT(meshIdx,min_idx)][reverseIdx]*newV;
            
            double diff1 = (localV - oldV).norm();
            
          
            if (diff1 > 1e-6) {
                std::cerr << this->timeCounter << " speed transformation error! " << diff1 << std::endl;
            }
           
            
#endif   
            // transform to the local tangent speed in the new plane
            localV = mesh->localtransform_v2v[meshIdx][min_idx]*localV;

            // transform to local coordinate in the new plane
            // because the local coordinate is on the edge of the old plane; it also must be in the edge of new plane
            
            particles[p_idx]->local_r = mesh->localtransform_p2p[meshIdx][min_idx](particles[p_idx]->local_r);
            particles[p_idx]->meshFaceIdx = newMeshIdx;
            
            
            if (abs(particles[p_idx]->local_r(0)) < positionPrecision) {
                particles[p_idx]->local_r(0) = 2 * positionPrecision;
            } else if (abs(particles[p_idx]->local_r(1)) < positionPrecision) {
                particles[p_idx]->local_r(1) = 2 * positionPrecision;
            } else if (abs((1 - particles[p_idx]->local_r.sum())) < positionPrecision){
                particles[p_idx]->local_r(1) = 1.0 - positionPrecision - particles[p_idx]->local_r(0);
                particles[p_idx]->local_r(0) = 1.0 - positionPrecision - particles[p_idx]->local_r(1);
                
            } else {
                std::cerr << this->timeCounter << " not in triangle after wrapping!" << std::endl;
                std::cout <<  particles[p_idx]->local_r << std::endl;
            }
#ifdef DEBUG
            if (!mesh->inTriangle(particles[p_idx]->local_r)){
                std::cerr << "not in triangleafter wrapping and adjustment " << std::endl;
            }
#endif            

        }
        
#ifdef DEBUG3
        particles[p_idx]->r = mesh->coord_l2g[particles[p_idx]->meshFaceIdx](particles[p_idx]->local_r);
        this->outputTrajectory(this->trajOs);
#endif        
        
        

    }



    if (!mesh->inTriangle(particles[p_idx]->local_r)){
        std::cerr << this->timeCounter << " not in triangle after the loop!" << std::endl;
        std::cout <<  particles[p_idx]->local_r << std::endl;
    }
    particles[p_idx]->r = mesh->coord_l2g[particles[p_idx]->meshFaceIdx](particles[p_idx]->local_r);
}


void Model::calForcesHelper(int i, int j, Eigen::Vector3d &F) {
    double dist;
    Eigen::Vector3d r;

    dist = 0.0;
    F.fill(0);
    r = particles[j]->r - particles[i]->r;
    dist = r.norm();
            
    if (dist < 2.0) {
        std::cerr << "overlap " << i << "\t" << j << "\t"<< this->timeCounter << "dist: " << dist << "\t" << this->timeCounter <<std::endl;

        dist = 2.06;
    }
    if (dist < cutoff) {
        // the unit of force is kg m s^-2
        // kappa here is kappa*a a non-dimensional number
        
        double Fpp = -4.0/3.0*
        Os_pressure*M_PI*(-3.0/4.0*pow(combinedSize,2.0)+3.0*dist*dist/16.0*radius_nm*radius_nm);
        Fpp = -Bpp * Kappa * exp(-Kappa*(dist-2.0));
        F = Fpp*r/dist;
    }
}

void Model::calForces() {
     
    for (int i = 0; i < numP; i++) {
        particles[i]->F.fill(0);
    }
    Eigen::Vector3d F;
    for (int i = 0; i < numP - 1; i++) {
        for (int j = i + 1; j < numP; j++) {
            calForcesHelper(i, j, F);
            particles[i]->F += F;
            particles[j]->F -= F;

        }
               
    }
    for (int i = 0; i < numP; i++) {
        particles[i]->F(2) -= parameter.fieldStrength;
    }
    
}
    


void Model::createInitialState(){

    this->readxyz(iniFile);
    std::stringstream ss;
    std::cout << "model initialize at round " << fileCounter << std::endl;
    ss << this->fileCounter++;
    if (trajOs.is_open()) trajOs.close();
    this->trajOs.open(filetag + "xyz_" + ss.str() + ".txt");
    this->timeCounter = 0;

              
}

void Model::outputTrajectory(std::ostream& os) {

    for (int i = 0; i < numP; i++) {
        os << i << "\t";
        for (int j = 0; j < 2; j++){
            os << particles[i]->local_r[j] << "\t";
        }
        os << particles[i]->meshFaceIdx << "\t";
        for (int j = 0; j < 3; j++){
            os << particles[i]->r(j) << "\t";
        }
        
        os << this->timeCounter*this->dt_ << "\t";
        os << std::endl;
    }
}


void Model::readxyz(const std::string filename) {
    std::ifstream is;
    is.open(filename.c_str());
    std::string line;
    double dum;
    for (int i = 0; i < numP; i++) {
        getline(is, line);
        std::stringstream linestream(line);
        linestream >> dum;
        linestream >> particles[i]->local_r(0);
        linestream >> particles[i]->local_r(1);
        linestream >> particles[i]->meshFaceIdx;
        
    }
    
    for (int i = 0; i < numP; i++) {
        particles[i]->r = mesh->coord_l2g[particles[i]->meshFaceIdx](particles[i]->local_r);        
    }
    
    is.close();
    
    // make sure that the initial configuration is not overlapped    
    
    for (int i = 0; i < numP - 1; i++){
        for (int j = i + 1; j < numP; j++){
            Eigen::Vector3d dist;
            dist = particles[i]->r - particles[j]->r;
            
            if (dist.norm() < parameter.cutoff) {
                std::cerr << "initial configuration too close!" << std::endl;
                
            }
            
        }
    }
    
}



