
#include "ElmerReader.h"
#include "LinearFEM.h"
#include "Materials.h"

#include "AdjointSensitivity.h"
#include "DirectSensitivity.h"
#include "TemporalInterpolationField.h"
#include "BoundaryFieldObjectiveFunction.h"
#include "ElasticMaterialParameterHandler.h"
#include "ViscousMaterialParameterHandler.h"

#include "../LBFGSpp/LBFGS.h"
//#include "../LBFGSpp/Newton.h"

#include "vtkUnstructuredGrid.h"

#include <cfloat> // Include this header for DBL_

using namespace MyFEM;
using std::ofstream;
using std::isnan;
using std::isinf;

class SimData{
public:
	LinearFEM theFEM;
	AverageBoundaryValueObjectiveFunction thePhi;
	std::vector<vtkSmartPointer<TemporalInterpolationField> > trackedTargets; std::vector<unsigned int> trackedIDs; // storage for target fields
	std::vector<vtkSmartPointer<TemporalInterpolationField> > markerData; std::vector<std::string> markerNames;
	std::vector<vtkSmartPointer<PositionRotationInterpolationField> > rbData; std::vector<std::string> rbNames; 
	double t_end; unsigned int tsteps;

	SimData(std::string meshFile, std::string mocapFile, std::string outFile,
		unsigned int bodyID, VectorField& g,
		double dt, double weight, double lameLamda, double lameMu, double viscNu, double viscH
	){
		loadMocapDataFromCSV(mocapFile, rbData,rbNames,markerData,markerNames, 1e-3 /*file units assumed in mm*/);

		// assign labelled data to mesh boundary IDs
		for(unsigned int i=0; i<markerNames.size(); ++i){
			if( markerNames[i].find("Cylinder:Top")!=std::string::npos ){
				trackedIDs.push_back(5); printf("\n%% found marker data for Cylinder:Top ");
				trackedTargets.push_back(markerData[i]);
			}else
			if( markerNames[i].find("Cylinder:1_1")!=std::string::npos ){
				trackedIDs.push_back(6); printf("\n%% found marker data for Cylinder:1_1 ");
				trackedTargets.push_back(markerData[i]);
			}else
			if( markerNames[i].find("Cylinder:1_2")!=std::string::npos ){
				trackedIDs.push_back(7); printf("\n%% found marker data for Cylinder:1_2 ");
				trackedTargets.push_back(markerData[i]);
			}else
			if( markerNames[i].find("Cylinder:1_3")!=std::string::npos ){
				trackedIDs.push_back(8); printf("\n%% found marker data for Cylinder:1_3 ");
				trackedTargets.push_back(markerData[i]);
			}else
			if( markerNames[i].find("Cylinder:2_1")!=std::string::npos ){
				trackedIDs.push_back(9); printf("\n%% found marker data for Cylinder:2_1 ");
				trackedTargets.push_back(markerData[i]);
			}else
			if( markerNames[i].find("Cylinder:2_2")!=std::string::npos ){
				trackedIDs.push_back(10); printf("\n%% found marker data for Cylinder:2_2 ");
				trackedTargets.push_back(markerData[i]);
			}else
			if( markerNames[i].find("Cylinder:2_3")!=std::string::npos ){
				trackedIDs.push_back(11); printf("\n%% found marker data for Cylinder:2_3 ");
				trackedTargets.push_back(markerData[i]);
			}else
			if( markerNames[i].find("Cylinder:3_1")!=std::string::npos ){
				trackedIDs.push_back(12); printf("\n%% found marker data for Cylinder:3_1 ");
				trackedTargets.push_back(markerData[i]);
			}else
			if( markerNames[i].find("Cylinder:3_2")!=std::string::npos ){
				trackedIDs.push_back(13); printf("\n%% found marker data for Cylinder:3_2 ");
				trackedTargets.push_back(markerData[i]);
			}else
			if( markerNames[i].find("Cylinder:3_3")!=std::string::npos ){
				trackedIDs.push_back(14); printf("\n%% found marker data for Cylinder:3_3 ");
				trackedTargets.push_back(markerData[i]);
			}else
			if( markerNames[i].find("Cylinder:4_1")!=std::string::npos ){
				trackedIDs.push_back(15); printf("\n%% found marker data for Cylinder:4_1 ");
				trackedTargets.push_back(markerData[i]);
			}else
			if( markerNames[i].find("Cylinder:4_2")!=std::string::npos ){
				trackedIDs.push_back(16); printf("\n%% found marker data for Cylinder:4_2 ");
				trackedTargets.push_back(markerData[i]);
			}else
			if( markerNames[i].find("Cylinder:4_3")!=std::string::npos ){
				trackedIDs.push_back(17); printf("\n%% found marker data for Cylinder:4_3 ");
				trackedTargets.push_back(markerData[i]);
			}
		}

		theFEM.useBDF2 = true; printf("\n%% Using BDF2 time integration "); // BDF2 works with both direct and adjoint sensitivity analysis - however accuracy of adjoint method is sometimes not great
		theFEM.loadMeshFromElmerFiles(meshFile);

		{	// adjust the time-range of the simulation to cover the data
			double trange[2];
			rbData[0]->getRange(trange);
			for(int i=0; i<rbData.size(); ++i) rbData[i]->t_shift = trange[0];
			for(int i=0; i<trackedTargets.size(); ++i) trackedTargets[i]->t_shift = trange[0];
			t_end=trange[1]-trange[0];
			tsteps = (t_end+0.5*dt)/dt;
			printf("\n%% Data time range (%.2lg %.2lg), simulated range (0 %.2lg), time step %.2lg (%u steps) ",trange[0],trange[1], t_end, dt, tsteps );
		}

		if( rbData.size()==1 ){ // only one clamp, assume it's attached on the lower boundary surface
			theFEM.setBoundaryCondition( 1, *(rbData[0]) ); // boundary ID 1 is the base face
			theFEM.setBoundaryCondition( 4, *(rbData[0]) ); // boundary ID 4 is the base marker
			// in order to get a more stable initial guess for the sim, we set the deformed coordinates of the entire mesh to the rigid-body transform described by rbBndryField at t==0
			for(unsigned int i=0; i<theFEM.getNumberOfNodes(); ++i){
				Eigen::Vector3d u;
				rbData[0]->eval(u , theFEM.getRestCoord(i), theFEM.getDeformedCoord(i), 0.0);
				theFEM.getDeformedCoord(i) = u;
			}
		}else{ // more clamps ...
			for(int i=0; i<rbNames.size(); ++i){
				if( rbNames[i].find("CylinderClamp1")!=std::string::npos ){
					printf("\n%% found boundary data for CylinderClamp1 ");
					theFEM.setBoundaryCondition( 1, *(rbData[i]) );
					theFEM.setBoundaryCondition( 4, *(rbData[i]) );
				}else
				if( rbNames[i].find("CylinderClamp2")!=std::string::npos ){
					printf("\n%% found boundary data for CylinderClamp2 ");
					rbData[i]->rc.y() = 0.18; // the second clamp is shifted by 18cm wrt. the origin of the model (where the first clamp is positioned)
					rbData[i]->p_shift.y() = -0.18;
					theFEM.setBoundaryCondition( 2, *(rbData[i]) );
					theFEM.setBoundaryCondition( 5, *(rbData[i]) );
				}
			}
			// in order to get a more stable initial guess for the sim, we set the deformed coordinates of the entire mesh to a blend of the rigid-body transforms at the boundaries for t==0
			Eigen::Vector3d u1,u2,x1,x2;
			theFEM.computeAverageRestCoordinateOfBoundary(1,x1);
			theFEM.computeAverageRestCoordinateOfBoundary(2,x2);
			for(unsigned int i=0; i<theFEM.getNumberOfNodes(); ++i){
				theFEM.bndCnd[1].data->eval(u1 , theFEM.getRestCoord(i), theFEM.getDeformedCoord(i), 0.0);
				theFEM.bndCnd[2].data->eval(u2 , theFEM.getRestCoord(i), theFEM.getDeformedCoord(i), 0.0);
				double a = (theFEM.getRestCoord(i)-x1).norm(), b = (theFEM.getRestCoord(i)-x2).norm();
				theFEM.getDeformedCoord(i) = (u1*b/(a+b)+u2*a/(a+b));
			}

		}

		// material model and body force
		double density = weight / theFEM.computeBodyVolume(); printf("\n%% density %.4lg ", density);
		theFEM.setExternalAcceleration(bodyID, g);
		/**/
		printf("\n\n%% optimizing Neohookean material ... \n");
		theFEM.initializeMaterialModel(bodyID, LinearFEM::ISOTROPIC_NEOHOOKEAN,lameLamda,lameMu,density);
		/*/
		printf("\n\n%% optimizing principal stretch material ... \n");
		vtkSmartPointer<HomogeneousMaterial> psMat = vtkSmartPointer<PrincipalStretchMaterial>::New();
		Eigen::VectorXd psMatParams( psMat->getNumberOfParameters() );
		psMat->setElasticParams(psMatParams.data(), lameLamda, lameMu); psMat->setDensity(psMatParams.data(), density);
		theFEM.initializeMaterialModel(bodyID,psMat, psMatParams.data());
		/**/

		/**/
		vtkSmartPointer<PowerLawViscosityModel> viscMdl = vtkSmartPointer<PowerLawViscosityModel>::New(); Eigen::VectorXd viscParam( viscMdl->getNumberOfParameters() ); viscParam.setZero();
		viscMdl->setViscosity( viscParam.data(), viscNu ); viscMdl->setPowerLawH( viscParam.data(), viscH );	
		theFEM.initializeViscosityModel(bodyID, viscMdl, viscParam.data() );
		/*/
		vtkSmartPointer<PowerSeriesViscosityModel> viscMdl = vtkSmartPointer<PowerSeriesViscosityModel>::New();
		viscMdl->powerIndices.resize(11); viscMdl->powerIndices.setLinSpaced(0.2,2.2);
		Eigen::VectorXd coeffs( viscMdl->getNumberOfParameters() ); coeffs.setOnes(); coeffs*=viscNu/(viscMdl->powerIndices.size());
		theFEM.initializeViscosityModel(bodyID, viscMdl,  coeffs.data() );
		cout << endl << "% power series viscous damping: flow indices are " << viscMdl->powerIndices.transpose() << ", coeffs are " << coeffs.transpose();
		/**/

		theFEM.setResetPoseFromCurrent(); // always use the current (rigidly transformed pose) during resets

		for(int i=0; i<trackedIDs.size(); ++i){ thePhi.addTargetField( trackedIDs[i] , trackedTargets[i] ); }
		// for debug: output the target displacements to VTU
		std::stringstream fileName;
		fileName.str(""); fileName.clear();
		fileName << outFile << "_targetBClocations";
		TemporalInterpolationField::writeFieldsToVTU(fileName.str(), 3*((t_end+0.5*dt)/dt), trackedTargets, trackedIDs);
		fileName.str(""); fileName.clear();
		fileName << outFile << "_transformedRestPose";
		theFEM.saveMeshToVTUFile(fileName.str(),false);
	}
};

class CombinedSensitivity{
public:
	SensitivityAnalysis& a; SensitivityAnalysis& b;
	double wA,wB;
	CombinedSensitivity(SensitivityAnalysis& first, SensitivityAnalysis& second, double firstWeight, double secondWeight) : a(first), b(second), wA(firstWeight), wB(secondWeight) {}
	virtual double operator()(const Eigen::VectorXd& params, Eigen::VectorXd& objectiveFunctionGradient){
		if( getEvalCounter()==0 ){
			bestRunPhiValue = DBL_MAX;
			bestRunParameters.resizeLike( params );
			bestRunGradient.resizeLike( params );
		}
		double phi, phiA=0.0, phiB=0.0;
		double scaleA=wA/a.getSimulatedTime(), scaleB=wB/b.getSimulatedTime();
		Eigen::VectorXd gA, gB;
		try{
			printf("\n%% Sim 1 ... (%.3lg) \n", scaleA);
			phiA = a(params,gA);
			if( isnan(gA.squaredNorm()) || isinf(gA.squaredNorm()) ) phiA = DBL_MAX;
		} catch (const std::exception& ex) {
			phiA = DBL_MAX; gA.setZero(); 
			printf("\n%% Sim 1 exception: \"%s\"\n", ex.what() );
		} catch (const std::string& ex) {
			phiA = DBL_MAX; gA.setZero(); 
			printf("\n%% Sim 1 error: \"%s\"\n", ex.c_str() );
		} catch (...) {
			phiA = DBL_MAX; gA.setZero(); 
			printf("\n%% Sim 1 unknown error \n");
		}
		try{
			printf("\n%% Sim 2 ... (%.3lg) \n", scaleB);
			phiB = b(params,gB);
			if( isnan(gB.squaredNorm()) || isinf(gB.squaredNorm()) ) phiB = DBL_MAX;
		} catch (const std::exception& ex) {
			phiB = DBL_MAX; gB.setZero(); 
			printf("\n%% Sim 2 exception: \"%s\"\n", ex.what() );
		} catch (const std::string& ex) {
			phiB = DBL_MAX; gB.setZero(); 
			printf("\n%% Sim 2 error: \"%s\"\n", ex.c_str() );
		} catch (...) {
			phiB = DBL_MAX; gB.setZero(); 
			printf("\n%% Sim 2 unknown error \n");
		}
		phi = (scaleA*phiA+scaleB*phiB);
		objectiveFunctionGradient.resizeLike(gA);
		objectiveFunctionGradient = scaleA*gA + scaleB*gB;

		printf("\n\n%% *************************\n");
		printf("\n%% combined obj.fcn. value %10.4lg (%10.4lg), grad.norm %10.4lg", phi,getEvalCounter()>1?(bestRunPhiValue-phi)/phi:1.0, objectiveFunctionGradient.norm() );
		printf("\n\n%% *************************\n");
		printf("\n\n%% *************************\n");
	
		if( phi < bestRunPhiValue ){
			bestRunPhiValue = phi;
			bestRunParameters = params;
			bestRunGradient = objectiveFunctionGradient;
		}

		return phi;
	}

	inline void resetEvalCounter(){ a.resetEvalCounter(); b.resetEvalCounter(); }
	inline unsigned int getEvalCounter(){ return a.getEvalCounter(); }

	double bestRunPhiValue;
	Eigen::VectorXd bestRunParameters, bestRunGradient;

};

int main_CylinderMarkers_dualPhi(int argc, char* argv[]){
	// args:
	//  [0]: bin file
	//  [1]: mesh file
	//  [2]: tracked target file -- skip param est. if ""
	//  [3]: sample weight -- default 9.3 g
	//  [4]: lameLambda -- default 10 kPa
	//  [5]: lameMu -- default 20 kPa
	//  [6]: viscNu -- default 12 Pas
	//  [7]: viscH  -- default 1
	//  [8]: framerate -- default 180 Hz

	bool doParamEst = true;
	bool doLocalParamEst = true; // optimize for per-element rather than global material parameters
	bool logParamMode = true; // only for LBFGS at the moment (very useful there though)
	bool noViscosityOpt = false; // only optimize elastic parameters, not viscosity -- not that useful
	std::string bndFileName, outFile="../_out/";
	std::stringstream fileName;

	double dt=1.0/180.0;
	double weight = 9.3e-3, // FlexFoam 5 sample has 9.3g, FlexFoam 3 one is 6.3g
		lameLamda = 10e3, lameMu = 20e3, viscNu = 12.0, viscH= 1.0; // initial guess for most optimization runs

	if( noViscosityOpt ){ viscNu = 2.652222586, viscH= 0.913059985; } // set more reasonable estimates (for FF5 sample)
	{
		double tmp;
		if( argc>3 ) if( sscanf(argv[3], "%lg", &tmp)==1) weight=tmp;
		if( argc>4 ) if( sscanf(argv[4], "%lg", &tmp)==1) lameLamda=tmp;
		if( argc>5 ) if( sscanf(argv[5], "%lg", &tmp)==1) lameMu=tmp;
		if( argc>6 ) if( sscanf(argv[6], "%lg", &tmp)==1) viscNu=tmp;
		if( argc>7 ) if( sscanf(argv[7], "%lg", &tmp)==1) viscH=tmp;
		if( argc>8 ) if( sscanf(argv[8], "%lg", &tmp)==1) dt=1/tmp; 
		printf("\n%% initial weight %.2lg, lambda %.2lg, mu %.2lg, nu %.2lg, h %.2lg, fps %.2lg", weight, lameLamda, lameMu, viscNu, viscH, 1.0/dt);
	}

	std::string f1,f2;
	if( argc>2 ){
		std::string s(argv[2]);
		size_t fileSep;
		if( s.at(0)=='(' && s.at(s.size()-1)==')' && (fileSep=s.find(','))!=s.npos ){ // correct format for two data files
			f1 = s.substr(1,fileSep-1);
			f2 = s.substr(fileSep+1,s.size()-fileSep-2);
			printf("\n%% reading data files: \n%% \"%s\" and  \n%% \"%s\"",  f1.c_str(), f2.c_str());
		}else{
			printf("\n%% expected mocap data files in format \"(filename1,filename2)\"");
			return -1;
		}

		size_t lastPathSep = f1.find_last_of("/\\")+1;
		size_t lastExtSep = f1.find_last_of(".");
		//printf("\n%% lastPathSep %d, lastExtSep %d, substr \"%s\"", lastPathSep,lastExtSep, s.substr(lastPathSep, lastExtSep-lastPathSep).c_str());
		outFile.append(s.substr(lastPathSep, lastExtSep-lastPathSep).c_str()).append("_dd/");
	}else printf("\n%% expected motion capture data file name ");
	{
		std::string s(argv[1]);
		size_t lastPathSep = s.find_last_of("/\\")+1;
		outFile.append(s.substr(lastPathSep));

#ifdef _WINDOWS
		std::string sc("for %f in (\""); sc.append(outFile).append("\") do mkdir %~dpf"); std::replace( sc.begin(), sc.end(), '/', '\\');
		int sr = system(sc.c_str());
#endif // _WINDOWS

	}
	// the main body of the mesh to simulate
	unsigned int bodyID=18;


	// gravity (-y)
	class GravityField : public VectorField{ public: virtual void eval(Eigen::Vector3d& g, const Eigen::Vector3d& x0, const Eigen::Vector3d& x, double t) const {
		g = -9.81 *Eigen::Vector3d::UnitY();
	} } g;

	LinearFEM::FORCE_BALANCE_EPS=1e-6; // reduce default accuracy a bit ...

	printf("\n\n%% -- CylinderMarkers -- Combined Mocap Data\n\n");
	printf("\n%% ... expecting one or two rigid bodies and markers labelled \"Cylinder:Top\" and \"Cylinder:1_1\" to \"Cylinder:4_3\"");


	SimData sim1(argv[1], f1, outFile + "_sim1_", bodyID, g, dt, weight, lameLamda, lameMu, viscNu, viscH );
	SimData sim2(argv[1], f2, outFile + "_sim2_", bodyID, g, dt, weight, lameLamda, lameMu, viscNu, viscH );
	
	if( doParamEst /*run parameter optimization*/){
		Eigen::VectorXd q, dphi;
		double phi = 0.0; int r = 0, ev = 0;
		LBFGSpp::LBFGSParam<double> optimOptions = defaultLBFGSoptions<LBFGSpp::LBFGSParam<double> >();
		optimOptions.epsilon = 1e-8;
		optimOptions.delta = 1e-5; // stop a bit earlier in term of objective function decrease (relative) per iteration
		ParameterHandler noOp(bodyID);

		if( doLocalParamEst ){ // won't work well if initial guess is too far away from optimum
			optimOptions.epsilon = 1e-14; // gradients per element will generally be lower than for global params, so run a bit longer
			printf("\n%% Per-element parameter optimization ... ");
			PerElementElasticMaterialParameterHandler elasticParamHandler(bodyID);
			//PerElementDensityMaterialParameterHandler densityParamHandler(bodyID);
			GlobalPLViscosityMaterialParameterHandler viscousParamHandler(bodyID);

			CombinedParameterHandler theQ(elasticParamHandler, noViscosityOpt ? noOp : viscousParamHandler );

			if( noViscosityOpt ){ printf("\n%% Only optimizing elastic parameters ");}
			if( logParamMode ){ theQ.useLogOfParams=true; printf("\n%% Using log-params for optimization ...\n"); }

			AdjointSensitivity sim1Sensitivity(sim1.thePhi, theQ, sim1.theFEM);
			AdjointSensitivity sim2Sensitivity(sim2.thePhi, theQ, sim2.theFEM);
			printf("\n%% Optimizing with LBFGS (adjoint sensitivity analysis) ");
			
			CombinedSensitivity theSensitivity(sim1Sensitivity,sim2Sensitivity,
				1.0/((double)sim1.trackedTargets.size()), 20.0/((double)sim2.trackedTargets.size())
			);
			LBFGSpp::LBFGSSolver<double> solver(optimOptions); //minimize with LBFGS

			sim1Sensitivity.setupDynamicSim(dt,sim1.tsteps, true , outFile + "_sim1_" );
			sim2Sensitivity.setupDynamicSim(dt,sim2.tsteps, true , outFile + "_sim2_" );

			q.resize( theQ.getNumberOfParams( sim1.theFEM ) );
			theQ.getCurrentParams(q, sim1.theFEM );
			r = solver.minimize(theSensitivity, q, phi);
		
			ev  += theSensitivity.getEvalCounter();
			phi  = theSensitivity.bestRunPhiValue;
			q    = theSensitivity.bestRunParameters;
			dphi = theSensitivity.bestRunGradient;

			theQ.setNewParams( theSensitivity.bestRunParameters, sim1.theFEM );
			theQ.setNewParams( theSensitivity.bestRunParameters, sim2.theFEM );
		}else{ // global param estimation
			/**/
			GlobalElasticMaterialParameterHandler	  elasticParamHandler(bodyID);
			/*/
			GlobalPrincipalStretchMaterialParameterHandler elasticParamHandler(bodyID);
			elasticParamHandler.alpha=5e-11; logParamMode=false; // need negative parameters in principal stretch material
			/**/

			GlobalPLViscosityMaterialParameterHandler viscousParamHandler(bodyID);
			CombinedParameterHandler theQ(elasticParamHandler, noViscosityOpt ? noOp : viscousParamHandler );

			if( noViscosityOpt ){ printf("\n%% Only optimizing elastic parameters ");}
			if( logParamMode ){ theQ.useLogOfParams=true; printf("\n%% Using log-params for optimization ...\n"); }

			AdjointSensitivity sim1Sensitivity(sim1.thePhi, theQ, sim1.theFEM);
			AdjointSensitivity sim2Sensitivity(sim2.thePhi, theQ, sim2.theFEM);
			printf("\n%% Optimizing with LBFGS (adjoint sensitivity analysis) ");
			//DirectSensitivity sim1Sensitivity(sim1.thePhi, theQ, sim1.theFEM);
			//DirectSensitivity sim2Sensitivity(sim2.thePhi, theQ, sim2.theFEM);
			//printf("\n%% Optimizing with LBFGS (direct sensitivity analysis) ");
			
			CombinedSensitivity theSensitivity(sim1Sensitivity,sim2Sensitivity,
				1.0/((double)sim1.trackedTargets.size()), 1.0/((double)sim2.trackedTargets.size())
			);
			LBFGSpp::LBFGSSolver<double> solver(optimOptions); //minimize with LBFGS
			


			sim1Sensitivity.setupDynamicSim(dt,sim1.tsteps, true , outFile + "_sim1_" );
			sim2Sensitivity.setupDynamicSim(dt,sim2.tsteps, true , outFile + "_sim2_" );

			q.resize( theQ.getNumberOfParams( sim1.theFEM ) );
			theQ.getCurrentParams(q, sim1.theFEM );
			r = solver.minimize(theSensitivity, q, phi);
		
			ev  += theSensitivity.getEvalCounter();
			phi  = theSensitivity.bestRunPhiValue;
			q    = theSensitivity.bestRunParameters;
			dphi = theSensitivity.bestRunGradient;

			theQ.setNewParams( theSensitivity.bestRunParameters, sim1.theFEM );
			theQ.setNewParams( theSensitivity.bestRunParameters, sim2.theFEM );
		}
		cout << endl << "% solver iterations " << r << ", fcn.evals " << ev;
		fileName.str(""); fileName.clear();
		fileName << outFile << "_optimResult.txt";
		ofstream optOut(fileName.str());
		optOut << "% solver iterations " << r << ", fcn.evals " << ev << ", objective function value " << phi << endl;
		optOut << endl << "% params:" << endl << q << endl;
		optOut << endl << "% gradient:" << endl << dphi << endl;
		optOut.close();

	}

	//if( 1 /* Forward sim and output result */ ){
		//theFEM.reset();

	//	printf("\n%% Forward sim - writing motion data ");
	//	id_set trackedBoundaries; // record average displacements for these boundary surface IDs
	//	for(int i=5; i<=17; ++i) trackedBoundaries.insert(i);
	//	fileName.str(""); fileName.clear();
	//	fileName << outFile << "_trackedBClocations.log";
	//	ofstream tbOut(fileName.str().c_str()); tbOut << std::setprecision(18);
	//	for(id_set::iterator tb=trackedBoundaries.begin(); tb!=trackedBoundaries.end(); ++tb){
	//		tbOut << *tb << " ";
	//	}	tbOut << endl;


	//	printf("\n%% init ");
	//	theFEM.assembleMassAndExternalForce();
	//	theFEM.updateAllBoundaryData();
	//	theFEM.staticSolve();
	//	// output tracked boundaries (avg. displacement)
	//	Eigen::Vector3d uB;
	//	tbOut << theFEM.simTime << " "; // should be 0.0 here
	//	for(id_set::iterator tb=trackedBoundaries.begin(); tb!=trackedBoundaries.end(); ++tb){
	//		theFEM.computeAverageDeformedCoordinateOfBoundary(*tb,uB);
	//		tbOut << uB[0] << " " << uB[1] << " " << uB[2] << " ";
	//	}	tbOut << endl;


	//	for(int step=0; step<tsteps; ++step){
	//		// write output file before step
	//		fileName.str(""); fileName.clear();
	//		fileName << outFile << "_" << std::setfill ('0') << std::setw(5) << step;
	//		theFEM.saveMeshToVTUFile(fileName.str());

	//		printf("\n%% step %5d/%d ", step+1,tsteps);
	//		theFEM.updateAllBoundaryData();
	//		theFEM.dynamicImplicitTimestep(dt);

	//		// output tracked boundaries (avg. displacement)
	//		tbOut << theFEM.simTime << " ";
	//		for(id_set::iterator tb=trackedBoundaries.begin(); tb!=trackedBoundaries.end(); ++tb){
	//			theFEM.computeAverageDeformedCoordinateOfBoundary(*tb,uB);
	//			tbOut << uB[0] << " " << uB[1] << " " << uB[2] << " ";
	//		}	tbOut << endl;
	//	}
	//	tbOut.close();

	//	// write last output file
	//	fileName.str(""); fileName.clear();
	//	fileName << outFile << "_" << std::setfill ('0') << std::setw(5) << tsteps;
	//	theFEM.saveMeshToVTUFile(fileName.str(), true);

	//	// readback tracked motion and save to VTU for visualization (ToDo: write first to TemporalInterpolationField objects and output directly from there - either text or VTU)
	//	fileName.str(""); fileName.clear();
	//	fileName << outFile << "_trackedBClocations.log";
	//	if( TemporalInterpolationField::buildFieldsFromTextFile( fileName.str(), trackedTargets, trackedIDs) < 0 ) return -1;
	//	fileName.str(""); fileName.clear();
	//	fileName << outFile << "_trackedBClocations";
	//	TemporalInterpolationField::writeFieldsToVTU(fileName.str(), 3*tsteps, trackedTargets, trackedIDs);
	//}

	printf("\n\n%% DONE.\n\n");

	return 0;
}
