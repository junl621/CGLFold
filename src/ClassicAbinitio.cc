// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file ClassicAbinitio.cc
/// @brief ab-initio fragment assembly protocol for proteins
/// @details
///   Contains currently: Classic Abinitio
///
///
/// @author Oliver Lange
/// @author James Thompson
/// @author Mike Tyka

// Unit Headers
#include <protocols/abinitio/ClassicAbinitio.hh>
#include <protocols/simple_moves/SymmetricFragmentMover.hh>

// Package Headers
#include <protocols/simple_moves/GunnCost.hh>

// Project Headers
#include <core/pose/Pose.hh>
#include <core/kinematics/MoveMap.hh>
#include <core/types.hh>
#include <core/scoring/ScoreFunction.fwd.hh>
#include <core/scoring/ScoreType.hh>
#include <core/scoring/ScoreFunctionFactory.hh>
#include <core/scoring/rms_util.hh>
#include <core/scoring/constraints/ConstraintSet.hh>
#include <core/scoring/constraints/ConstraintSet.fwd.hh>

#include <protocols/moves/Mover.hh>
#include <protocols/simple_moves/BackboneMover.hh>
#include <protocols/moves/MoverContainer.hh>
#include <protocols/moves/TrialMover.hh>
#include <protocols/moves/RepeatMover.hh>
#include <protocols/moves/WhileMover.hh>
#include <protocols/abinitio/AllResiduesChanged.hh>

//for cenrot
#include <protocols/moves/CompositionMover.hh>
#include <core/pack/task/PackerTask.hh>
#include <core/pack/task/TaskFactory.hh>
#include <core/pack/task/operation/TaskOperations.hh>
#include <protocols/minimization_packing/PackRotamersMover.hh>
#include <basic/options/keys/corrections.OptionKeys.gen.hh>
#include <protocols/simple_moves/SwitchResidueTypeSetMover.hh>
//#include <protocols/simple_moves/BackboneMover.hh>


// ObjexxFCL Headers
#include <ObjexxFCL/string.functions.hh>

// Utility headers
#include <utility/exit.hh>
#include <utility/vector1.fwd.hh>
#include <utility/pointer/ReferenceCount.hh>
#include <utility/io/ozstream.hh>
#include <numeric/numeric.functions.hh>
#include <basic/prof.hh>
#include <basic/Tracer.hh>
#include <basic/options/option.hh>
#include <basic/options/keys/abinitio.OptionKeys.gen.hh>
#include <basic/options/keys/run.OptionKeys.gen.hh>
#include <basic/options/keys/templates.OptionKeys.gen.hh>

//// C++ headers
#include <cstdlib>
#include <string>
#ifdef WIN32
#include <ctime>
#endif

//debug

#include <protocols/moves/MonteCarlo.hh>
#include <utility/vector0.hh>
#include <utility/vector1.hh>

///====================================================headers===================================================
//#include <protocols/abinitio/LJImplementation.fwd.hh>
//#include <protocols/abinitio/LJImplementation.hh>

#include <protocols/abinitio/LJAngleRotation.fwd.hh>
#include <protocols/abinitio/LJAngleRotation.hh>

#include <core/scoring/ScoringManager.hh>
#include <core/scoring/Ramachandran.hh>

#include <protocols/simple_moves/FragmentMover.fwd.hh>

///=========================== @CGLFold ===============================
///@note 用户添加程序
#include<iostream>
#include<fstream>
#include<sstream>
#include<vector>
#include<map>
#include<algorithm>
//#include<unordered_map>
#include<set>
#include<math.h>
///@brief make_pair头文件 pair
#include<utility>

#include <numeric/random/random.hh>
///@brief 产生随机数
#include<stdlib.h>
#include<time.h>
///@brief 计算rmsd的函数所在的头文件
#include <core/scoring/rms_util.hh>
///@brief 控制输出格式
#include<iomanip>
///@brief 读取天然态蛋白质
#include <core/import_pose/import_pose.hh>
#include <basic/options/keys/in.OptionKeys.gen.hh>
///@brief 输出pdb文件
#include <basic/options/keys/out.OptionKeys.gen.hh>
///@brief 计算二级结构元素的距离
//#include <core/pose/Pose.hh>
#include <core/conformation/Residue.hh>
#include <core/chemical/Atom.hh>
#include <numeric/xyzVector.hh>

#include <core/id/AtomID.hh>
#include <core/id/TorsionID.hh>

#include <core/kinematics/MoveMap.hh>
#include <utility/pointer/owning_ptr.hh>
#include <memory>

#include<stdlib.h>

///ctreate a new direct
#include<sys/stat.h>
#include<sys/types.h>

using namespace std;

core::pose::Pose initPose;
core::pose::Pose nativePose;
///=========================== @CGLFold ===============================

static basic::Tracer tr( "protocols.abinitio" );

using core::Real;
using namespace core;
using namespace basic;
using namespace basic::options;
using namespace basic::options::OptionKeys;

/*!
@detail call this:
ClassicAbinitio::register_options() before devel::init().
Derived classes that overload this function should also call Parent::register_options()
*/

// This method of adding options with macros is a pain in the ass for people
// trying to nest ClassicAbinitio as part of other protocols. If you don't call
// ClassicAbinitio::register_options() in your main function, you get a really
// unintuitive segfault as the options system doesn't know about the options
// listed below. The solution is to call register_options() in your main method
// before devel::init(), which is really ugly as the main method shouldn't need
// to know what protocols are called, and it's prone to error because it's an
// easy thing to forget.
// This should get some more thought before it becomes the standard way to add options.

void protocols::abinitio::ClassicAbinitio::register_options() {
	Parent::register_options();
	using namespace basic::options;
	using namespace basic::options::OptionKeys;
	option.add_relevant( OptionKeys::abinitio::increase_cycles );
	option.add_relevant( OptionKeys::abinitio::smooth_cycles_only );
	option.add_relevant( OptionKeys::abinitio::debug );
	option.add_relevant( OptionKeys::abinitio::skip_convergence_check );
	option.add_relevant( OptionKeys::abinitio::log_frags );
	option.add_relevant( OptionKeys::abinitio::only_stage1 );
	option.add_relevant( OptionKeys::abinitio::end_bias );
	option.add_relevant( OptionKeys::abinitio::symmetry_residue );
	option.add_relevant( OptionKeys::abinitio::vdw_weight_stage1 );
	option.add_relevant( OptionKeys::abinitio::override_vdw_all_stages );
	option.add_relevant( OptionKeys::abinitio::recover_low_in_stages );
	option.add_relevant( OptionKeys::abinitio::close_chbrk );
}


namespace protocols {
namespace abinitio {
  
    Real ClassicAbinitio::TrialEnergy = 0.0;
  
///============================== @CGLFold ===============================================
  typedef id::TorsionType TorsionType;
  typedef std::pair< Size, TorsionType > MoveMapTorsionID;
///============================== @CGLFold ===============================================
  
//little helper function
bool contains_stageid( utility::vector1< ClassicAbinitio::StageID > vec, ClassicAbinitio::StageID query ) {
	return find( vec.begin(), vec.end(), query) != vec.end();
}

/// @detail  large (stage1/stage2)
/// small(stage2/stage3/stage4)
/// smooth_small ( stage3/stage4)
ClassicAbinitio::ClassicAbinitio(
	simple_moves::FragmentMoverOP brute_move_small,
	simple_moves::FragmentMoverOP brute_move_large,
	simple_moves::FragmentMoverOP smooth_move_small,
	int  /*dummy otherwise the two constructors are ambiguous */
) :
	brute_move_small_(std::move( brute_move_small )),
	brute_move_large_( brute_move_large ),
	smooth_move_small_(std::move( smooth_move_small ))
{
	BaseClass::type( "ClassicAbinitio" );
	get_checkpoints().set_type("ClassicAbinitio");
	// std::cerr << "ClassicAbinitio::constructor has stubbed out...(fatal) see code file";
	// runtime_assert( 0 ); //---> needs more implementation to use this constructor: e.g. read out movemap from FragmentMover...
	movemap_ = brute_move_large->movemap();
	//  set_defaults( pose ); in constructor virtual functions are not called
	bSkipStage1_ = false;
	bSkipStage2_ = false;

	close_chbrk_ = false;

	stage4_cycles_pack_rate_ = 0.25;
}

ClassicAbinitio::ClassicAbinitio(
	core::fragment::FragSetCOP fragset_small,
	core::fragment::FragSetCOP fragset_large,
	core::kinematics::MoveMapCOP movemap
)  :
	movemap_( movemap )
{
	BaseClass::type( "ClassicAbinitio" );
	get_checkpoints().set_type("ClassicAbinitio");
	using namespace basic::options;
	simple_moves::ClassicFragmentMoverOP bms, bml, sms;
	using simple_moves::FragmentCostOP;
	using simple_moves::ClassicFragmentMover;
	using simple_moves::SymmetricFragmentMover;
	using simple_moves::SmoothFragmentMover;
	using simple_moves::SmoothSymmetricFragmentMover;
	using simple_moves::GunnCost;
	if ( option[ OptionKeys::abinitio::log_frags ].user() ) {
		if ( !option[ OptionKeys::abinitio::debug ] ) utility_exit_with_message( "apply option abinitio::log_frags always together with abinitio::debug!!!");
		bms = simple_moves::ClassicFragmentMoverOP( new simple_moves::LoggedFragmentMover( fragset_small, movemap ) );
		bml = simple_moves::ClassicFragmentMoverOP( new simple_moves::LoggedFragmentMover( fragset_large, movemap ) );
		sms = simple_moves::ClassicFragmentMoverOP( new SmoothFragmentMover( fragset_small, movemap, FragmentCostOP( new GunnCost ) ) );
	} else if ( option[ OptionKeys::abinitio::symmetry_residue ].user() ) {
		Size const sr (  option[ OptionKeys::abinitio::symmetry_residue ] );
		bms = simple_moves::ClassicFragmentMoverOP( new SymmetricFragmentMover( fragset_small, movemap, sr ) );
		bml = simple_moves::ClassicFragmentMoverOP( new SymmetricFragmentMover( fragset_large, movemap, sr ) );
		sms = simple_moves::ClassicFragmentMoverOP( new SmoothSymmetricFragmentMover( fragset_small, movemap, FragmentCostOP( new GunnCost ), sr ) );
	} else {
		bms = simple_moves::ClassicFragmentMoverOP( new ClassicFragmentMover( fragset_small, movemap ) );
		bml = simple_moves::ClassicFragmentMoverOP( new ClassicFragmentMover( fragset_large, movemap ) );
		sms = simple_moves::ClassicFragmentMoverOP( new SmoothFragmentMover ( fragset_small, movemap, FragmentCostOP( new GunnCost ) ) );
	}

	bms->set_end_bias( option[ OptionKeys::abinitio::end_bias ] ); //default is 30.0
	bml->set_end_bias( option[ OptionKeys::abinitio::end_bias ] );
	sms->set_end_bias( option[ OptionKeys::abinitio::end_bias ] );

	brute_move_small_ = bms;
	brute_move_large_ = bml;
	smooth_move_small_ = sms;

	using namespace core::pack::task;
	//init the packer
	pack_rotamers_ = minimization_packing::PackRotamersMoverOP( new protocols::minimization_packing::PackRotamersMover() );
	TaskFactoryOP main_task_factory( new TaskFactory );
	main_task_factory->push_back( operation::TaskOperationCOP( new operation::RestrictToRepacking ) );
	//main_task_factory->push_back( new operation::PreserveCBeta );
	pack_rotamers_->task_factory(main_task_factory);

	bSkipStage1_ = false;
	bSkipStage2_ = false;

	close_chbrk_ = false;

	stage4_cycles_pack_rate_ = 0.25;
}

/// @details Call parent's copy constructor and perform a shallow
/// copy of all the data.  NOTE: Shallow copy is only to preserve
/// behavior pre 9/7/2009 when the compiler-provided copy constructor
/// was being invoked.
ClassicAbinitio::ClassicAbinitio( ClassicAbinitio const & src ) :
	//utility::pointer::ReferenceCount(),
	Parent( src )
{
	stage1_cycles_ = src.stage1_cycles_;
	stage2_cycles_ = src.stage2_cycles_;
	stage3_cycles_ = src.stage3_cycles_;
	stage4_cycles_ = src.stage4_cycles_;
	stage5_cycles_ = src.stage5_cycles_;
	score_stage1_ = src.score_stage1_;
	score_stage2_ = src.score_stage2_;
	score_stage3a_ = src.score_stage3a_;
	score_stage3b_ = src.score_stage3b_;
	score_stage4_ = src.score_stage4_;
	score_stage4rot_ = src.score_stage4rot_;
	score_stage5_ = src.score_stage5_;
	apply_large_frags_ = src.apply_large_frags_;
	short_insert_region_ = src.short_insert_region_;
	just_smooth_cycles_ = src.just_smooth_cycles_;
	bQuickTest_ = src.bQuickTest_;
	close_chbrk_ = src.close_chbrk_;
	temperature_ = src.temperature_;
	movemap_ = src.movemap_;
	mc_ = src.mc_;
	brute_move_small_ = src.brute_move_small_;
	brute_move_large_ = src.brute_move_large_;
	smooth_move_small_ = src.smooth_move_small_;
	trial_large_ = src.trial_large_;
	trial_small_ = src.trial_small_;
	smooth_trial_small_ = src.smooth_trial_small_;
	total_trials_ = src.total_trials_;
	bSkipStage1_ = src.bSkipStage1_;
	bSkipStage2_ = src.bSkipStage2_;
	bSkipStage3_ = src.bSkipStage3_;
	bSkipStage4_ = src.bSkipStage4_;
	bSkipStage5_ = src.bSkipStage5_;
	recover_low_stages_ = src.recover_low_stages_;
}

/// @brief Explicit destructor is needed to destroy all the OPs
/// The compiler does all the work, but it requires that we place
/// the destructor in the .cc file.
ClassicAbinitio::~ClassicAbinitio() = default;

/// @brief setup moves, mc-object, scores
/// @details can't call this from constructor; virtual functions don't operate until construction has completed.

void
ClassicAbinitio::init( core::pose::Pose const& pose ) {
	// Parent::init( pose );
	set_defaults( pose );
	// bInitialized_ = true;
}

/// @brief ClassicAbinitio has virtual functions... use this to obtain a new instance
moves::MoverOP
ClassicAbinitio::clone() const
{
	return moves::MoverOP( new ClassicAbinitio( *this ) );
}

///======================================================================================================
///=========== @CGLFold ========================== @CGLFold ======================== @CGLFold ===========
void 
ClassicAbinitio::read_parameters(){
	ifstream informParam("./parameter_list");
	string line;
	while (getline(informParam,line)){
		if ( line[0] != '#' && line[0] != ' ' ){
			istringstream line_data( line );
			string parameterName;
			Real parameterValue;
			line_data >> parameterName >> parameterValue;
			parametersMap.insert(make_pair(parameterName, parameterValue));
		}
	}
	cout << parametersMap.size() << endl;
	informParam.close();
}

void ClassicAbinitio::get_parameters(map<string, Real>& parametersMap){
	if (parametersMap.find("NP") != parametersMap.end()){
	    NP_ = parametersMap["NP"];
	    NP_reciprocal = (Real)1 / NP_;
	    cout << "### 1 / NP = " << NP_reciprocal << endl;
	}
	else{
	    cout << "====================================parameter 'NP' is not found in parameters!!!" << endl;
	    exit(0);
	}
	
	if (parametersMap.find("G") != parametersMap.end())
	    G_ = parametersMap["G"];
	else{
	    cout << "====================================parameter 'G' is not found in parameters!!!" << endl;
	    exit(0);
	}
	
	if (parametersMap.find("KTc") != parametersMap.end()){
	    KTc_ = 1 / parametersMap["KTc"];
	    cout << "### 1 / KTc = " << KTc_ << endl;
	}
	else{
	    cout << "====================================parameter 'KT_Cscore' is not found in parameters!!!" << endl;
	    exit(0);
	}
	
	if (parametersMap.find("Only_Global") != parametersMap.end())
	    Only_Global_ = parametersMap["Only_Global"];
	else{
	    cout << "====================================parameter 'Only_Global' is not found in parameters!!!" << endl;
	    exit(0);
	}
	
	convergence_length_ = 80;
}

void 
ClassicAbinitio::read_contact(){
	ifstream read_contact("./output_files/filtered_cmap.txt");
	string contact_line;
	while (getline(read_contact, contact_line))
	{
	    Contact contact;
	    istringstream Data(contact_line);
	    
	    Data >> contact.residue1;
	    Data >> contact.residue2;
	    Data >> contact.confidence;
	    Contacts.push_back(contact);
	    
	    Cscore_full += -pow(8.0, contact.confidence);
	}
	if ( Contacts.size() == 0 ){
		cout << "ERROR!!!\n" << "ERROR : Read Contact failed!!!\n" << "ERROR!!!" << endl;
		exit(0);
	}
	else
		cout << "Contact Full Mark = " << Cscore_full << endl;
}

Real 
ClassicAbinitio::Contact_score(core::pose::Pose& pose){
	Real Cscore( 0 );
	for (Size i = 0; i < Contacts.size(); ++i){
		Size residue1( Contacts[i].residue1 );
		Size residue2( Contacts[i].residue2 );
		Real confidence ( Contacts[i].confidence );
		
		Real distance( 0 );
		if ( pose.residue(residue1).name3() == "GLY" || pose.residue(residue2).name3() == "GLY" ){
			numeric::xyzVector<Real> CA1 = pose.residue(residue1).xyz("CA");
			numeric::xyzVector<Real> CA2 = pose.residue(residue2).xyz("CA");
			distance = CA1.distance(CA2);
		}else{
			numeric::xyzVector<Real> CB1 = pose.residue(residue1).xyz("CB");
			numeric::xyzVector<Real> CB2 = pose.residue(residue2).xyz("CB");
			distance = CB1.distance(CB2);
		}
		
		if ( distance <= 3.8 )
			Cscore += pow(8.0, (3.8 - distance) );
		else if (distance <= 8)
			Cscore += -pow(8.0, confidence);
		else
			Cscore += pow(8.0, confidence) * log(distance - 8.0 + 1.0);
	}
	return Cscore;
}

Real 
ClassicAbinitio::Average_Cscore_of_Populatin(vector< Real >& population_Cscore){
	Real total_Cscore( 0 );
	for (Size i = 0; i < NP_; ++i)
		total_Cscore += population_Cscore[i];
	Real average_Cscore( total_Cscore * NP_reciprocal );
	
	return average_Cscore;
}

Real 
ClassicAbinitio::Average_Cscore_of_Populatin(Real& old_Cscore, Real& new_Cscore, Real& average_Cscore){
	return average_Cscore + (new_Cscore - old_Cscore) * NP_reciprocal;
}

bool 
ClassicAbinitio::boltzmann_accept(Real const& targetEnergy, Real const& trialEnergy){
	if ( trialEnergy <= targetEnergy )
		return true;
	else{
		if ( exp( -(trialEnergy - targetEnergy) * 0.5 ) < numeric::random::rg().uniform() )
			return false;
		else
			return true;
	}
}

bool 
ClassicAbinitio::boltzmann_accept(const Real& targetEnergy, const Real& trialEnergy, const Real& recipocal_KT){
	/// recipocal_KT = 1 / KT
	if ( trialEnergy <= targetEnergy )
		return true;
	else{
		if ( exp( -(trialEnergy - targetEnergy) * recipocal_KT ) < numeric::random::rg().uniform() )
			return false;
		else
			return true;
	}
}

void 
ClassicAbinitio::Output_SPICKER_All_Data(core::pose::Pose& pose, const Real& energy){
	++Spicker_All_num;
	if ( Spicker_All_num > 20000 ){
		Spicker_All_num = 1;
		SPICKER_All_Data.close();
		++Spicker_All_file_num;
		string file_name;
		stringstream ss;
		ss << "./output_files/SPICKER_data/spicker.data" << Spicker_All_file_num;
		ss >> file_name;
		SPICKER_All_Data.open( file_name.c_str() );
	}
	Size Num_resdue( pose.total_residue() );
	SPICKER_All_Data << right << setw(8) << Num_resdue
		    << right << setw(10) << setprecision(3) << fixed << energy
		    << right << setw(8) << Spicker_All_num
		    << right << setw(8) << Spicker_All_num
		    << endl;
	for ( Size r = 1; r <= Num_resdue; ++r ){
		numeric::xyzVector<Real> CA_ = pose.residue(r).xyz("CA");
		
		SPICKER_All_Data << right << setw(10) << setprecision(3) << fixed << CA_.x()
			    << right << setw(10) << setprecision(3) << fixed << CA_.y()
			    << right << setw(10) << setprecision(3) << fixed << CA_.z()
			    << endl;
	}
}

void 
ClassicAbinitio::SPICKER_Demand_All(core::pose::Pose& nativePose){
	SPICKER_All_Data.close();
  
	ofstream tra("./output_files/SPICKER_data/tra.in");
	tra << " " << Spicker_All_file_num << " 1 1" << endl;
	for (Size n = 1; n <= Spicker_All_file_num; ++n)
		tra << "spicker.data" << n << endl;
	tra.close();
	
	ofstream rmsinp("./output_files/SPICKER_data/rmsinp");
	rmsinp << 1 << "  " << proteinLength << "    ! these two numbers indicate region for RMSD calculations [1," << proteinLength << "]"
		<< "\n" << proteinLength << "       ! protein length";
	rmsinp.close();
	
	ofstream seq("./output_files/SPICKER_data/seq.dat");
	for (Size r = 1; r <= proteinLength; ++r)
		seq << right << setw(5) << r 
		    << right << setw(6) << nativePose.residue(r).name3() 
		    << endl;
	seq.close();
	
	ofstream CA("./output_files/SPICKER_data/CA");
	for (Size r = 1; r <= proteinLength; ++r){
		numeric::xyzVector<Real> CA_ = nativePose.residue(r).xyz("CA");
	  
		CA << "ATOM"
		  << right << setw(7) << r
		  << right << setw(4) << "CA"
		  << right << setw(5) << nativePose.residue(r).name3()
		  << right << setw(6) << r
		  << right << setw(12) << CA_.x()
		  << right << setw(8) << CA_.y()
		  << right << setw(8) << CA_.z()
		  << endl;
	}
	CA.close();
}

vector< core::pose::Pose > 
ClassicAbinitio::generate_population(const Size& population_size){
	using namespace moves;
	using namespace scoring;
	using namespace scoring::constraints;
	
	vector< core::pose::Pose > population;
	core::pose::Pose init_pose(initPose);
	bool success(true);
	if ( !bSkipStage1_ ) {
	//	stage3_to_stage4 = false;
		PROF_START( basic::STAGE1 );
		clock_t starttime = clock();
		if ( !prepare_stage1( init_pose ) ) {
			cout << "ERROR!!!\n" << "ERROR : prepare_stage1 failed!!!\n" << "ERROR!!!" << endl;
			exit(0);
		}
		tr.Info <<  "\n===================================================================\n";
		tr.Info <<  "   Stage 1                                                         \n";
		tr.Info <<  "   Folding with score0 for max of " << stage1_cycles() << std::endl;

		if ( option[ basic::options::OptionKeys::run::profile ] ) prof_show();
		if ( option[ basic::options::OptionKeys::abinitio::debug ]() ) {
			output_debug_structure( init_pose, "stage0" );
		}
		if ( !get_checkpoints().recover_checkpoint( init_pose, get_current_tag(), "stage_1", false /* fullatom*/, true /*fold tree */ ) ) {
			ConstraintSetOP orig_constraints(NULL);
			orig_constraints = init_pose.constraint_set()->clone();
			
///=======================================用户添加程序=====================================================================
			for (Size i = 0; i < population_size; ++i){
			    core::pose::Pose new_pose( init_pose );
			    mc_->reset( new_pose );
			    success = do_stage1_cycles( new_pose );
			    recover_low( new_pose, STAGE_1 );
			    population.push_back(new_pose);
			}
///======================================================================================================================
			if ( tr.Info.visible() ) current_scorefxn().show( tr, init_pose );
			mc().show_counters();
			total_trials_+=mc().total_trials();
			mc().reset_counters();

			init_pose.constraint_set( orig_constraints ); // restore constraints - this is critical for checkpointing to work
			get_checkpoints().checkpoint( init_pose, get_current_tag(), "stage_1", true /*fold tree */ );
		} //recover checkpoint
		get_checkpoints().debug( get_current_tag(), "stage_1", current_scorefxn()( init_pose ) );

		clock_t endtime = clock();
		PROF_STOP( basic::STAGE1 );
		if ( option[ basic::options::OptionKeys::run::profile ] ) prof_show();
		if ( option[ basic::options::OptionKeys::abinitio::debug ]() ) {
			tr.Info << "Timeperstep: " << (double(endtime) - starttime )/(CLOCKS_PER_SEC ) << std::endl;
			output_debug_structure( init_pose, "stage1" );
		}
		tr.Info <<  "\n===================================================================\n";
		tr.Info <<  "  Finished Abinitio Starge1!!!" << endl;
		tr.Info <<  endl;
	} //skipStage1
	if ( !success ) {
		cout << "ERROR!!!\n" << "ERROR : prepare_stage1 failed!!!\n" << "ERROR!!!" << endl;
		exit(0);
	}
	if ( !bSkipStage2_ ) {
	//	stage3_to_stage4 = true;
		
		tr.Info <<  "\n===================================================================\n";
		tr.Info <<  "   Stage 2                                                         \n";
		tr.Info <<  "   Folding with score1 for " << stage2_cycles() << std::endl;

		PROF_START( basic::STAGE2 );
		clock_t starttime = clock();
		if ( close_chbrk_ ) {
			Real const setting( 0.25 );
			set_score_weight( scoring::linear_chainbreak, setting, STAGE_2 );
			tr.Info <<  " Chain_break score assigned " << std::endl;
		}
		if ( !prepare_stage2( init_pose ) )  {
			cout << "ERROR!!!\n" << "ERROR : prepare_stage1 failed!!!\n" << "ERROR!!!" << endl;
			exit(0);
		}
		if ( !get_checkpoints().recover_checkpoint( init_pose, get_current_tag(), "stage_2", false /* fullatom */, true /*fold tree */ ) ) {
			ConstraintSetOP orig_constraints(NULL);
			orig_constraints = init_pose.constraint_set()->clone();	
			
///=======================================用户添加程序===============================stage2================================
			for (Size i = 0; i < NP_; ++i){
				mc_->reset( population[i] );
				success = do_stage2_cycles( population[i] );
				recover_low( population[i], STAGE_2 );
			}
///======================================================================================================================                 
			if  ( tr.visible() ) current_scorefxn().show( tr, init_pose );
			mc().show_counters();
			total_trials_+=mc().total_trials();
			mc().reset_counters();
			init_pose.constraint_set( orig_constraints ); // restore constraints - this is critical for checkpointing to work
			get_checkpoints().checkpoint( init_pose, get_current_tag(), "stage_2", true /*fold tree */ );
		}
		get_checkpoints().debug( get_current_tag(), "stage_2", current_scorefxn()( init_pose ) );

		clock_t endtime = clock();
		PROF_STOP( basic::STAGE2 );
		if ( option[ basic::options::OptionKeys::run::profile ] ) prof_show();
		if ( option[ basic::options::OptionKeys::abinitio::debug ]() ) {
			output_debug_structure( init_pose, "stage2" );
			tr << "Timeperstep: " << (double(endtime) - starttime )/(CLOCKS_PER_SEC ) << std::endl;
		}
		tr.Info <<  "\n===================================================================\n";
		tr.Info <<  "  Finished Abinitio Starge2!!!" << endl;
		tr.Info <<  endl;
	} //bSkipStage2
	if ( !success ) {
		cout << "ERROR!!!\n" << "ERROR : prepare_stage1 failed!!!\n" << "ERROR!!!" << endl;
		exit(0);
	}
	if ( !bSkipStage3_ ) {
	//	stage3_to_stage4 = true;
		
		tr.Info <<  "\n===================================================================\n";
		tr.Info <<  "   Stage 3                                                         \n";
		tr.Info <<  "   Folding with score2 and score5 for " << stage3_cycles() <<std::endl;

		PROF_START( basic::STAGE3 );
		clock_t starttime = clock();

		if ( !prepare_stage3( init_pose ) ) {
			cout << "ERROR!!!\n" << "ERROR : prepare_stage1 failed!!!\n" << "ERROR!!!" << endl;
			exit(0);
		}
	///=======================================用户添加程序===============
		for (Size i = 0; i < NP_; ++i)
		{
		    mc_->reset( population[i] );
		    success = do_stage3_cycles( population[i] );
		    recover_low( population[i], STAGE_3b );
		}
	///================================================================
		if ( tr.Info.visible() ) current_scorefxn().show( tr, init_pose );
		mc().show_counters();
		total_trials_+=mc().total_trials();
		mc().reset_counters();

		clock_t endtime = clock();
		PROF_STOP( basic::STAGE3);
		if ( option[ basic::options::OptionKeys::run::profile ] ) prof_show();
		if ( option[ basic::options::OptionKeys::abinitio::debug ]() ) {
			output_debug_structure( init_pose, "stage3" );
			tr << "Timeperstep: " << (double(endtime) - starttime )/( CLOCKS_PER_SEC) << std::endl;
		}
		tr.Info <<  "\n===================================================================\n";
		tr.Info <<  "  Finished Abinitio Starge3!!!" << endl;
		tr.Info <<  endl;
	}
	if ( !success ) {
		cout << "ERROR!!!\n" << "ERROR : prepare_stage1 failed!!!\n" << "ERROR!!!" << endl;
		exit(0);
	}
	if ( !bSkipStage4_ ) {
	//	stage3_to_stage4 = true;
		
		tr.Info <<  "\n===================================================================\n";
		tr.Info <<  "   Stage 4                                                         \n";
		tr.Info <<  "   Folding with score3 for " << stage4_cycles() <<std::endl;

		PROF_START( basic::STAGE4 );
		clock_t starttime = clock();

		if ( !prepare_stage4( init_pose ) ) {
			cout << "ERROR!!!\n" << "ERROR : prepare_stage1 failed!!!\n" << "ERROR!!!" << endl;
			exit(0);
		}
	///=======================================用户添加程序===============
		for (Size i = 0; i < NP_; ++i)
		{
		    mc_->reset( population[i] );
		    success = do_stage4_cycles( population[i] );
		    recover_low( population[i], STAGE_4 );
		}
	///===============================================================
		if ( tr.Info.visible() ) current_scorefxn().show( tr, init_pose);
		mc().show_counters();
		total_trials_+=mc().total_trials();
		mc().reset_counters();

		clock_t endtime = clock();
		PROF_STOP( basic::STAGE4 );
		if ( option[ basic::options::OptionKeys::run::profile ] ) prof_show();
		if ( option[ basic::options::OptionKeys::abinitio::debug ]() ) {
			output_debug_structure( init_pose, "stage4" );
			tr << "Timeperstep: " << (double(endtime) - starttime )/( CLOCKS_PER_SEC ) << std::endl;
		}
		tr.Info <<  "\n===================================================================\n";
		tr.Info <<  "  Finished Abinitio Starge4!!!"<< endl;
		tr.Info <<  endl;
	}
	
	return population;
}

vector< Real > 
ClassicAbinitio::calculate_population_energy(vector< core::pose::Pose >& population){
	vector<Real> population_energy;
	
	for (Size i = 0; i < NP_; ++i){
		TrialEnergy = (*score_stage4_)(population[i]);
		if ( find_if( population_energy.begin(), population_energy.end(), isSimilar ) == population_energy.end() ){
			population_energy.push_back(TrialEnergy);
		}
		else{
			cout << "The TrialPose is Similar to The Pose in Population!!!" << endl;
 			cout << "Restarting a New Trajectory :" << endl;
			bool flag = false;
			while ( !flag ){
				core::pose::Pose new_pose( generate_a_new_pose() );
				
				TrialEnergy = (*score_stage4_)(new_pose);
				
				if ( find_if( population_energy.begin(), population_energy.end(), isSimilar ) == population_energy.end() ){
					population[i] = new_pose;
					population_energy.push_back(TrialEnergy);
					flag = true;
				}
			}
		}
	}
	if ( population_energy.size() != NP_ ){
		cout << "ERROR!!!\n" << "ERROR : calculate population energy failed!!!\n" << "ERROR!!!" << endl;
		exit(0);
	}
	return population_energy;
}

vector< Real > 
ClassicAbinitio::calculate_population_Cscore(vector< core::pose::Pose >& population){
	vector<Real> population_Cscore;
	
	for (Size i = 0; i < NP_; ++i)
		population_Cscore.push_back( Contact_score( population[i] ) );
		
	if ( population_Cscore.size() != NP_ ){
		cout << "ERROR!!!\n" << "ERROR : calculate population Cscore failed!!!\n" << "ERROR!!!" << endl;
		exit(0);
	}
	return population_Cscore;
}

core::pose::Pose 
ClassicAbinitio::generate_a_new_pose(){
	core::pose::Pose new_pose( initPose );
	
	bool success(true);
	if ( !bSkipStage1_ ){
		if ( !prepare_stage1( new_pose ) ) {
			cout << "ERROR!!!\n" << "ERROR : prepare_stage1 failed!!!\n" << "ERROR!!!" << endl;
			exit(0);
		}
	//	stage3_to_stage4 = false;
		success = do_stage1_cycles( new_pose );
		recover_low( new_pose, STAGE_1 );
		
		if ( !success ) {
			cout << "ERROR!!!\n" << "ERROR : do_stage1_cycles failed!!!\n" << "ERROR!!!" << endl;
			exit(0);
		}
	}
	
	if ( !bSkipStage2_ ){
		if ( !prepare_stage2( new_pose ) ) {
			cout << "ERROR!!!\n" << "ERROR : prepare_stage2 failed!!!\n" << "ERROR!!!" << endl;
			exit(0);
		}
	//	stage3_to_stage4 = true;
		success = do_stage2_cycles( new_pose );
		recover_low( new_pose, STAGE_2 );
		
		if ( !success ) {
			cout << "ERROR!!!\n" << "ERROR : do_stage2_cycles failed!!!\n" << "ERROR!!!" << endl;
			exit(0);
		}
	}
	
	if ( !bSkipStage3_ ){
		if ( !prepare_stage3( new_pose ) ) {
			cout << "ERROR!!!\n" << "ERROR : prepare_stage3 failed!!!\n" << "ERROR!!!" << endl;
			exit(0);
		}
	//	stage3_to_stage4 = true;
		success = do_stage3_cycles( new_pose );
		recover_low( new_pose, STAGE_3b );
		
		if ( !success ) {
			cout << "ERROR!!!\n" << "ERROR : do_stage3_cycles failed!!!\n" << "ERROR!!!" << endl;
			exit(0);
		}
	}
	
	if ( !bSkipStage4_ ){
		if ( !prepare_stage4( new_pose ) ) {
			cout << "ERROR!!!\n" << "ERROR : prepare_stage4 failed!!!\n" << "ERROR!!!" << endl;
			exit(0);
		}
	//	stage3_to_stage4 = true;
		success = do_stage4_cycles( new_pose );
		recover_low( new_pose, STAGE_4 );
		
		if ( !success ) {
			cout << "ERROR!!!\n" << "ERROR : do_stage4_cycles failed!!!\n" << "ERROR!!!" << endl;
			exit(0);
		}
	}
	
	return new_pose;
}

void 
ClassicAbinitio::Global_search(vector< core::pose::Pose >& population, vector< Real >& population_energy, vector<Real>& population_Cscore){
    ///=================================== @Global_Search ================================================
    for ( Size i = 0; i < NP_; ++i){
	///====================== @Fragment_recombination =====================
	Size base( numeric::random::rg().random_range(0, NP_ - 1) );
	Size rand2( numeric::random::rg().random_range(0, NP_ - 1) );
	Size rand3( numeric::random::rg().random_range(0, NP_ - 1) );
	while ( base == i )
		base = numeric::random::rg().random_range(0, NP_ - 1);
	while ( rand2 == base || rand2 == i )
		rand2 = numeric::random::rg().random_range(0, NP_ - 1);
	while ( rand3 == base || rand3 == rand2 || rand3 == i )
		rand3 = numeric::random::rg().random_range(0, NP_ - 1);
	
	core::pose::Pose base_pose( population[base] );
	core::pose::Pose rand2_pose( population[rand2] );
	core::pose::Pose rand3_pose( population[rand3] );
	
	Size rand_w1( numeric::random::rg().random_range(1, proteinLength - frag_ + 1) );
	Size rand_w2( numeric::random::rg().random_range(1, proteinLength - frag_ + 1) );
	Size rand_w3( numeric::random::rg().random_range(1, proteinLength - frag_ + 1) );
	while ( rand_w2 == rand_w1 )
	    rand_w2 = numeric::random::rg().random_range(1, proteinLength - frag_ + 1);
	while ( rand_w3 == rand_w1 || rand_w3 == rand_w2 )
	    rand_w3 = numeric::random::rg().random_range(1, proteinLength - frag_ + 1);
	
	for (Size r = rand_w1; r < rand_w1 + frag_; ++r){
	    base_pose.set_phi( r, rand2_pose.phi( r ) );
	    base_pose.set_psi( r, rand2_pose.psi( r ) );
	    base_pose.set_omega( r, rand2_pose.omega( r ) );
	}
	for (Size r = rand_w2; r < rand_w2 + frag_; ++r){
	    base_pose.set_phi( r, rand3_pose.phi( r ) );
	    base_pose.set_psi( r, rand3_pose.psi( r ) );
	    base_pose.set_omega( r, rand3_pose.omega( r ) );
	}
	for (Size r = rand_w3; r < rand_w3 + frag_; ++r){
	    base_pose.set_phi( r, population[i].phi( r ) );
	    base_pose.set_psi( r, population[i].psi( r ) );
	    base_pose.set_omega( r, population[i].omega( r ) );
	}
	///==================== @Fragment_assemble ========================
	Real baseEnergy( (*score_stage4_)(base_pose) );
	core::pose::Pose trialPose;
	Real trialEnergy;
	Size trialTimes ( 0 );
	while ( trialTimes < 200 ){
	    trialPose = base_pose;
	    FragAssem_->apply( trialPose );
	    trialEnergy = (*score_stage4_)(trialPose);
	    if ( boltzmann_accept(baseEnergy, trialEnergy) )
		break;
	    ++trialTimes;
	}
	if ( trialTimes >= 200 ){
//	    cout << "### Fragment Assemble Failed Afeter 200 trials!!!" << endl;
	    trialPose = base_pose;
	    trialEnergy = baseEnergy;
	}
	///================= @Individual_Selection ================ @Individual_Selection ================
	Real trialCscore( Contact_score(trialPose) );
	if ( Global_Selection_Mode_ == 1 ){
	    TrialEnergy = trialEnergy;
	    if ( find_if( population_energy.begin(), population_energy.end(), isSimilar ) == population_energy.end() ){
		if ( boltzmann_accept(population_energy[i], trialEnergy) ){
		    population[i] = trialPose;
		    population_energy[i] = trialEnergy;
		    Ave_Cscore = Average_Cscore_of_Populatin( population_Cscore[i], trialCscore, Ave_Cscore );
		    population_Cscore[i] = trialCscore;
		    
		    Output_SPICKER_All_Data(trialPose, trialEnergy);
		}
		else if ( boltzmann_accept(population_Cscore[i], trialCscore, KTc_) ){
		    population[i] = trialPose;
		    population_energy[i] = trialEnergy;
		    Ave_Cscore = Average_Cscore_of_Populatin( population_Cscore[i], trialCscore, Ave_Cscore );
		    population_Cscore[i] = trialCscore;
		    
		    Output_SPICKER_All_Data(trialPose, trialEnergy);
		}
	    }
	}
	else if ( Global_Selection_Mode_ == 2 ){
	    TrialEnergy = trialEnergy;
	    if ( find_if( population_energy.begin(), population_energy.end(), isSimilar ) == population_energy.end() ){
		if ( boltzmann_accept(population_Cscore[i], trialCscore, KTc_) ){
		    population[i] = trialPose;
		    population_energy[i] = trialEnergy;
		    Ave_Cscore = Average_Cscore_of_Populatin( population_Cscore[i], trialCscore, Ave_Cscore );
		    population_Cscore[i] = trialCscore;
		    
		    Output_SPICKER_All_Data(trialPose, trialEnergy);
		}
		else if ( boltzmann_accept(Ave_Cscore, trialCscore, KTc_) && boltzmann_accept(population_energy[i], trialEnergy) ){
		    population[i] = trialPose;
		    population_energy[i] = trialEnergy;
		    Ave_Cscore = Average_Cscore_of_Populatin( population_Cscore[i], trialCscore, Ave_Cscore );
		    population_Cscore[i] = trialCscore;
		    
		    Output_SPICKER_All_Data(trialPose, trialEnergy);
		}
	    }
	}
	else{
		cout << "### Global_Selection_Mode must equal to '1' or '2' !!!" << endl;
		cout << "### Global_Selection_Mode = " << Global_Selection_Mode_ << endl;
		exit(0);
	}
    }
}

void 
ClassicAbinitio::Local_search(vector< core::pose::Pose >& population, vector< Real >& population_energy, vector<Real>& population_Cscore, LJAngleRotationOP LJAR){
    ///=================================== @Global_Search ================================================
    for ( Size i = 0; i < NP_; ++i){
	vector<std::pair<Size, Size> > LoopRegions;
	string SS( population[i].secstruct() );
	Size count( 0 );
	for (Size j = 0; j < SS.size(); ++j){
	    if ( SS[j] == 'L' )
		++count;
	    else{
		if ( count >= 4 )
		    LoopRegions.push_back( make_pair( j - count + 1, j ) );
		count = 0;	
	    }		
	}
	multimap<Real, core::pose::Pose> candidates_map;
	if ( LoopRegions.size() != 0 ){
	    ///@brief randomly select a loop region
	    Size loopRand = numeric::random::rg().random_range(0, LoopRegions.size() - 1);
	    Size loopBegin = LoopRegions[loopRand].first;
	    Size loopEnd = LoopRegions[loopRand].second;
	    
	    if ( loopBegin == 1 || loopEnd == proteinLength ){
		for (Size n = 1; n <= 10; ++n){
		    core::pose::Pose trialPose( population[i] );
		    for (Size r = loopBegin; r <= loopEnd; ++r){
			trialPose.set_phi(r, trialPose.phi(r) + ( numeric::random::rg().uniform() * 10 - 5 ) );
			trialPose.set_psi(r, trialPose.psi(r) + ( numeric::random::rg().uniform() * 10 - 5 ) );
		    }
		    candidates_map.insert( make_pair( (*score_stage4_)(trialPose) , trialPose ) );
		}
	    }
	    else{
		///@note rotation point set
		vector<vector<Real> > targetPoints_CA;
		for (Size r = loopEnd + 1; r <= proteinLength; ++r){
		    numeric::xyzVector<Real> CA = population[i].residue(r).xyz("CA");
		    vector<Real> cordinate;
		    cordinate.push_back( CA.x() );
		    cordinate.push_back( CA.y() );
		    cordinate.push_back( CA.z() );
		    
		    targetPoints_CA.push_back( cordinate );
		}
		///@note rotation axis set
		vector<vector<Real> > rotationAxis;
		bool add_a_angle = true;
		if ( add_a_angle ){
		    numeric::xyzVector<Real> CA = population[i].residue(loopBegin - 1).xyz("CA");
		    numeric::xyzVector<Real> C = population[i].residue(loopBegin - 1).atom("C").xyz();
		    vector<double> axis;
		    double x = (C.x() - CA.x()) / sqrt( pow((C.x() - CA.x()), 2) + pow((C.y() - CA.y()), 2) + pow((C.z() - CA.z()), 2));
		    double y = (C.y() - CA.y()) / sqrt( pow((C.x() - CA.x()), 2) + pow((C.y() - CA.y()), 2) + pow((C.z() - CA.z()), 2));
		    double z = (C.z() - CA.z()) / sqrt( pow((C.x() - CA.x()), 2) + pow((C.y() - CA.y()), 2) + pow((C.z() - CA.z()), 2));
		    axis.push_back(x);
		    axis.push_back(y);
		    axis.push_back(z);
		    rotationAxis.push_back(axis);
		}
		
		for (Size r = loopBegin; r <= loopEnd; ++r){
		    numeric::xyzVector<Real> N = population[i].residue(r).atom("N").xyz();
		    numeric::xyzVector<Real> CA = population[i].residue(r).xyz("CA");
		    numeric::xyzVector<Real> C = population[i].residue(r).atom("C").xyz();
		    
		    vector<double> axis_1;
		    double x_1 = (CA.x() - N.x()) / sqrt( pow((CA.x() - N.x()), 2) + pow((CA.y() - N.y()), 2) + pow((CA.z() - N.z()), 2));
		    double y_1 = (CA.y() - N.y()) / sqrt( pow((CA.x() - N.x()), 2) + pow((CA.y() - N.y()), 2) + pow((CA.z() - N.z()), 2));
		    double z_1 = (CA.z() - N.z()) / sqrt( pow((CA.x() - N.x()), 2) + pow((CA.y() - N.y()), 2) + pow((CA.z() - N.z()), 2));
		    axis_1.push_back(x_1);
		    axis_1.push_back(y_1);
		    axis_1.push_back(z_1);
		    rotationAxis.push_back(axis_1);
		    
		    vector<double> axis_2;
		    double x_2 = (C.x() - CA.x()) / sqrt( pow((C.x() - CA.x()), 2) + pow((C.y() - CA.y()), 2) + pow((C.z() - CA.z()), 2));
		    double y_2 = (C.y() - CA.y()) / sqrt( pow((C.x() - CA.x()), 2) + pow((C.y() - CA.y()), 2) + pow((C.z() - CA.z()), 2));
		    double z_2 = (C.z() - CA.z()) / sqrt( pow((C.x() - CA.x()), 2) + pow((C.y() - CA.y()), 2) + pow((C.z() - CA.z()), 2));
		    axis_2.push_back(x_2);
		    axis_2.push_back(y_2);
		    axis_2.push_back(z_2);
		    rotationAxis.push_back(axis_2);
		}
		///@note disturbance angles search
		LJAR->rotation_parameters(targetPoints_CA, rotationAxis);
		vector<vector<Real> > Top10_Disturbance_Angles = LJAR->angle_learning();
		
		for (Size a = 0; a < Top10_Disturbance_Angles.size(); ++a){
		    core::pose::Pose trialPose( population[i] );
		    
		    Size angle(0);
		    trialPose.set_psi(loopBegin-1, trialPose.psi(loopBegin-1) + Top10_Disturbance_Angles[a][angle]);
		    ++angle;
		    for (Size b = loopBegin; b <= loopEnd; ++b){
			    trialPose.set_phi(b, trialPose.phi(b) + Top10_Disturbance_Angles[a][angle]);
			    ++angle;
			    trialPose.set_psi(b, trialPose.psi(b) + Top10_Disturbance_Angles[a][angle]);
			    ++angle;
		    }
		    candidates_map.insert( make_pair( (*score_stage4_)(trialPose) , trialPose ) );
		}
	    }
	}
	else
		cout << "WARRNING!!!\n" << "WARRNING : No appropriately loop region to perturbance !!!\n" << endl;
	
	///@note comformation update
	Size count2( 0 );
	for ( multimap<Real, core::pose::Pose>::iterator iter = candidates_map.begin(); count2 < candidates_map.size(); ++iter, ++count2){
	    Real trialEnergy( iter->first );
	    Real trialCscore( Contact_score(iter->second) );
	    if ( Local_Selection_Mode_ == 1 ){
		if ( boltzmann_accept( population_energy[i], trialEnergy ) ){
		    population[i] = iter->second;
		    population_energy[i] = trialEnergy;
		    Ave_Cscore = Average_Cscore_of_Populatin( population_Cscore[i], trialCscore, Ave_Cscore );
		    population_Cscore[i] = trialCscore;
		    
		    Output_SPICKER_All_Data(population[i], trialEnergy);
		   
		    break;
		}
		else if ( trialCscore < population_Cscore[i] ){
		    population[i] = iter->second;
		    population_energy[i] = trialEnergy;
		    Ave_Cscore = Average_Cscore_of_Populatin( population_Cscore[i], trialCscore, Ave_Cscore );
		    population_Cscore[i] = trialCscore;
		    
		    Output_SPICKER_All_Data(population[i], trialEnergy);
		    
		    break;
		}
	    }
	    else if ( Local_Selection_Mode_ == 2 ){
		if ( trialCscore < population_Cscore[i] ){
		    population[i] = iter->second;
		    population_energy[i] = trialEnergy;
		    Ave_Cscore = Average_Cscore_of_Populatin( population_Cscore[i], trialCscore, Ave_Cscore );
		    population_Cscore[i] = trialCscore;
		    
		    Output_SPICKER_All_Data(population[i], trialEnergy);
		    
		    break;
		}
		else if ( boltzmann_accept(Ave_Cscore, trialCscore, KTc_) && boltzmann_accept( population_energy[i], iter->first ) ){
		    population[i] = iter->second;
		    population_energy[i] = trialEnergy;
		    Ave_Cscore = Average_Cscore_of_Populatin( population_Cscore[i], trialCscore, Ave_Cscore );
		    population_Cscore[i] = trialCscore;
		    
		    Output_SPICKER_All_Data(population[i], trialEnergy);
		    
		    break;
		}
	    }
	    else{
		cout << "### Local_Selection_Mode must equal to '1' or '2' !!!" << endl;
		cout << "### Local_Selection_Mode = " << Local_Selection_Mode_ << endl;
		exit(0);
	    }
	}
    }
}

void 
ClassicAbinitio::Output_last_generation(vector< core::pose::Pose >& population, vector< Real >& population_energy, vector<Real>& population_Cscore){
    
    Size min_E_index( 0 );
    Real min_E( 10000);
    Size min_C_index( 0 );
    Real min_C( 10000 );
    cout << "# min_E " << min_E << " min_C " << min_C << endl;
    
    for ( Size p = 0; p < NP_; ++p ){
	    if ( population_energy[p] < min_E ){
		    min_E = population_energy[p];
		    min_E_index = p;
	    }
	    if ( population_Cscore[p] < min_C ){
		    min_C = population_Cscore[p];
		    min_C_index = p;
		    
	    }
    }
   
    cout << "# min_E " << min_E_index << " " << min_E << " min_C " << min_C_index << " " << min_C << endl;
    population[min_C_index].dump_pdb("output_files/model_3.pdb");
    population[min_E_index].dump_pdb("output_files/model_4.pdb");
   /*  
    ofstream last_generation("last_generation_data");
    for ( Size p = 0; p < NP_; ++p ){
	last_generation << right << setw(3) << p + 1
	    << right << setw(12) << setprecision(3) << fixed << population_Cscore[p]
	    << right << setw(12) << setprecision(3) << fixed << population_energy[p]
	    << right << setw(12) << setprecision(3) << fixed << core::scoring::CA_rmsd( nativePose, population[p] )
	    << endl;
    }
    last_generation.close();*/
}


void ClassicAbinitio::apply( pose::Pose & pose ) {
	using namespace moves;
	using namespace scoring;
	using namespace scoring::constraints;

	Parent::apply( pose );
	if ( option[ OptionKeys::run::dry_run ]() ) return;



	bool success( true );
	total_trials_ = 0;

///======== @CGLFold ============================ @CGLFold ============================== @CGLFold ========
///===== @BEGIN =========== @BEGIN ============ @BEGIN ============ @BEGIN ============ @BEGIN ============
	///using Rosetta stage1 and stage2 for populatin initiolization
	bSkipStage1_ = false;
	bSkipStage2_ = false;
	bSkipStage3_ = true;
	bSkipStage4_ = true;
	
	//Instantiate a angle search object
	LJAngleRotationOP LJAR( new LJAngleRotation );
	
	//read and extract parameters
	read_parameters();
	get_parameters(parametersMap);
	LJAR->get_parameters(parametersMap);
	
	read_contact();
	
	proteinLength = pose.total_residue();
	initPose = pose;
	
	///@note Spicker Parameters
	Spicker_All_num = 0;
	Spicker_All_file_num = 1;
	SPICKER_All_Data.open("./output_files/SPICKER_data/spicker.data1");
		
///=========================================================================================================	
	///@brief create a initial population and calculate its energy and Cscore
	vector<core::pose::Pose> population( generate_population(NP_) );
	vector<Real> population_energy( calculate_population_energy(population) );
	vector<Real> population_Cscore( calculate_population_Cscore(population) );
	
	Ave_Cscore = Average_Cscore_of_Populatin( population_Cscore );
	
	frag_ = 9;
	FragAssem_ = brute_move_large_;
	Global_Selection_Mode_ = 1;
	Local_Selection_Mode_ = 1;
	cout << "#  Fragment recombination and assembly with " << 9 << "-mer fragment" << endl;
	cout << "#  Global exploration Selection Mode: G_E_C" << endl;
	cout << "#  Local perturbation Selection Mode: L_E_C" << endl;
	
	bool finish( false );
	vector<Real> average_Cscore_region(convergence_length_, 0.0);
	
	if ( !Only_Global_ )
	    cout << "#  CGLFold: global exploration + loop perturbation" << endl;
	else
	    cout << "#  CGFold: only global exploration, no loop perturbation" << endl;

	for (Size g = 1; g <= G_; ++g){
		cout << "#  generation: " << g << endl;
		if ( g == 0.25 * G_ + 1 ){
		    Global_Selection_Mode_ = 2;
		    Local_Selection_Mode_ = 2;
	//	    cout << "#  Global Selection Mode: G_C_E" << endl;
	//	    cout << "#  Local Selection Mode: L_C_E" << endl;
		}
		if ( g == 0.5 * G_ + 1  ){
		    frag_ = 3;
		    FragAssem_ = brute_move_small_;
	//	    cout << "#  Fragment recombination and assembly with " << 3 << "-mer fragment" << endl;
		}
		if ( g == G_ )
		    finish = true;
		
		///@brief Global exploration
		Global_search(population, population_energy, population_Cscore);
		
		///@brief Loop perturbation
		if ( !Only_Global_ )
		    Local_search(population, population_energy, population_Cscore, LJAR);
				
		if ( (g <= 0.5 * G_ || g > 0.5 * G_ + convergence_length_) && Ave_Cscore <= 0.80 * Cscore_full ){
			Size n( 0 );
			for (; n < average_Cscore_region.size(); ++n){
				if ( fabs( Ave_Cscore - average_Cscore_region[n] ) > 1 )
					break;
			}
			if ( n == average_Cscore_region.size() ){
				if ( g <= 0.5 * G_ )
					g = 0.5 * G_;
				else
					finish = true;
			}
		}
		
		if ( finish ){
			Output_last_generation(population, population_energy, population_Cscore);
		  
			cout << "#  Cscore Convergence in generation : " << g << endl;
			break;
		}
		
		Size k( g % convergence_length_ );
		average_Cscore_region[k] = Ave_Cscore;
	}
		
	SPICKER_Demand_All(initPose);

///======== @END ============ @END ============== @END ============= @END ============ @END ===============
///======== @CGLFold ============================ @CGLFold ============================== @CGLFold ========

	return;
}// ClassicAbinitio::apply( pose::Pose & pose )


std::string
ClassicAbinitio::get_name() const {
	return "ClassicAbinitio";
}

//@brief return FramgentMover for smooth_small fragment insertions (i.e., stage4 moves)
simple_moves::FragmentMoverOP
ClassicAbinitio::smooth_move_small() {
	return smooth_move_small_;
}

//@brief return FragmentMover for small fragment insertions ( i.e., stage3/4 moves )
simple_moves::FragmentMoverOP
ClassicAbinitio::brute_move_small() {
	return brute_move_small_;
}

//@brief return FragmentMover for large fragment insertions (i.e., stage1/2 moves )
simple_moves::FragmentMoverOP
ClassicAbinitio::brute_move_large() {
	return brute_move_large_;
}

//@brief change the movemap ( is propagated to mover-objects )
//@detail overload if your extension stores additional moves as member variables
void
ClassicAbinitio::set_movemap( core::kinematics::MoveMapCOP mm )
{
	movemap_ = mm;
	if ( smooth_move_small_ ) smooth_move_small_->set_movemap( mm );
	if ( brute_move_small_  ) brute_move_small_ ->set_movemap( mm );
	if ( brute_move_large_  ) brute_move_large_ ->set_movemap( mm );
}

//@brief set new instances of FragmentMovers
void
ClassicAbinitio::set_moves(
	simple_moves::FragmentMoverOP brute_move_small,
	simple_moves::FragmentMoverOP brute_move_large,
	simple_moves::FragmentMoverOP smooth_move_small
)
{
	smooth_move_small_ = smooth_move_small;
	brute_move_small_  = brute_move_small;
	brute_move_large_  = brute_move_large;
	update_moves();
}

//@brief returns current movemap
core::kinematics::MoveMapCOP
ClassicAbinitio::movemap() {
	return movemap_;
}

//@detail read cmd_line options and set default versions for many protocol members: trials/moves, score-functions, Monte-Carlo
void ClassicAbinitio::set_defaults( pose::Pose const& pose ) {
	temperature_ = 2.0;
	bSkipStage1_ = false;
	bSkipStage2_ = false;
	bSkipStage3_ = false;
	bSkipStage4_ = false;
	bSkipStage5_ = true; //vats is turned off by default
	set_default_scores();
	set_default_options();
	set_default_mc( pose, *score_stage1_ );
	update_moves();
}

//@detail called to notify about changes in Movers: new movemap or Moverclass
void ClassicAbinitio::update_moves() {
	/* set apply_large_frags_ and
	short_insert_region_
	*/
	/* what about move-map ? It can be set manually for all Fragment_Moves .. */
	// set_move_map();
	set_trials();
}

//@detail create instances of TrialMover for our FragmentMover objects
void ClassicAbinitio::set_trials() {
	// setup loop1
	runtime_assert( brute_move_large_ != nullptr );
	trial_large_ = moves::TrialMoverOP( new moves::TrialMover( brute_move_large_, mc_ ) );
	//trial_large_->set_keep_stats( true );
	trial_large_->keep_stats_type( moves::accept_reject );

	runtime_assert( brute_move_small_ != nullptr );
	trial_small_ = moves::TrialMoverOP( new moves::TrialMover( brute_move_small_, mc_ ) );
	//trial_small_->set_keep_stats( true );
	trial_small_->keep_stats_type( moves::accept_reject );

	runtime_assert( smooth_move_small_ != nullptr );
	smooth_trial_small_ = moves::TrialMoverOP( new moves::TrialMover( smooth_move_small_, mc_ ) );
	//smooth_trial_small_->set_keep_stats( true );
	smooth_trial_small_->keep_stats_type( moves::accept_reject );

	//build trial_pack mover
	moves::SequenceMoverOP combo_small( new moves::SequenceMover() );
	combo_small->add_mover(brute_move_small_);
	combo_small->add_mover(pack_rotamers_);
	trial_small_pack_ = moves::TrialMoverOP( new moves::TrialMover(combo_small, mc_) );
	moves::SequenceMoverOP combo_smooth( new moves::SequenceMover() );
	combo_smooth->add_mover(smooth_move_small_);
	combo_smooth->add_mover(pack_rotamers_);
	smooth_trial_small_pack_ = moves::TrialMoverOP( new moves::TrialMover(combo_smooth, mc_) );
}

//@detail sets Monto-Carlo object to default
void ClassicAbinitio::set_default_mc(
	pose::Pose const & pose,
	scoring::ScoreFunction const & scorefxn
) {
	set_mc( moves::MonteCarloOP( new moves::MonteCarlo( pose, scorefxn, temperature_ ) ) );
}

//@detail sets Monto-Carlo object
void ClassicAbinitio::set_mc( moves::MonteCarloOP mc_in ) {
	mc_ = mc_in;
	if ( trial_large_ ) trial_large_->set_mc( mc_ );
	if ( trial_small_ ) trial_small_->set_mc( mc_ );
	if ( smooth_trial_small_ ) smooth_trial_small_->set_mc( mc_ );
}

//@detail override cmd-line setting for "increase_cycling"
void ClassicAbinitio::set_cycles( Real increase_cycles ) {
	stage1_cycles_ = static_cast< int > (2000 * increase_cycles);
	stage2_cycles_ = static_cast< int > (2000 * increase_cycles);
	stage3_cycles_ = static_cast< int > (2000 * increase_cycles);
	stage4_cycles_ = static_cast< int > (4000 * increase_cycles);
	stage5_cycles_ = static_cast< int > (50000* increase_cycles);//vats

	using namespace basic::options;
	if ( option[ OptionKeys::abinitio::only_stage1 ]() ) {
		stage2_cycles_ = 0;
		stage3_cycles_ = 0;
		stage4_cycles_ = 0;
		bSkipStage2_ = bSkipStage3_ = /*bSkipStage3_ =*/ true;  // Was bSkipStage4_ meant? ~Labonte
	}
}

void ClassicAbinitio::set_default_scores() {
	using namespace scoring;
	using namespace basic::options;
	using namespace basic::options::OptionKeys;

	tr.Debug << "creating standard scoring functions" << std::endl;

	if ( option[ OptionKeys::abinitio::stage1_patch ].user() ) {
		score_stage1_  = ScoreFunctionFactory::create_score_function( "score0", option[ OptionKeys::abinitio::stage1_patch ]() );
	} else {
		score_stage1_  = ScoreFunctionFactory::create_score_function( "score0" );
	}

	if ( option[ OptionKeys::abinitio::stage2_patch ].user() ) {
		score_stage2_  = ScoreFunctionFactory::create_score_function( "score1", option[ OptionKeys::abinitio::stage2_patch ]() );
	} else {
		score_stage2_  = ScoreFunctionFactory::create_score_function( "score1" );
	}

	if ( option[ OptionKeys::abinitio::stage3a_patch ].user() ) {
		score_stage3a_ = ScoreFunctionFactory::create_score_function( "score2", option[ OptionKeys::abinitio::stage3a_patch ]() );
	} else {
		score_stage3a_ = ScoreFunctionFactory::create_score_function( "score2" );
	}

	if ( option[ OptionKeys::abinitio::stage3b_patch ].user() ) {
		score_stage3b_ = ScoreFunctionFactory::create_score_function( "score5", option[ OptionKeys::abinitio::stage3b_patch ]() );
	} else {
		score_stage3b_ = ScoreFunctionFactory::create_score_function( "score5" );
	}

	if ( option[ OptionKeys::abinitio::stage4_patch ].user() ) {
		score_stage4_  = ScoreFunctionFactory::create_score_function( "score3", option[ OptionKeys::abinitio::stage4_patch ]() );
	} else {
		score_stage4_  = ScoreFunctionFactory::create_score_function( "score3" );
	}

	//loading the cenrot score
	score_stage4rot_ = ScoreFunctionFactory::create_score_function( "score4_cenrot_relax" );
	//score_stage4rot_->set_weight(core::scoring::cen_rot_dun, 0.0);
	score_stage4rot_sc_ = ScoreFunctionFactory::create_score_function( "score4_cenrot_repack" );
	//score_stage4rot_sc_->set_weight(core::scoring::cen_rot_dun, 1.0);

	if ( option[ OptionKeys::abinitio::stage5_patch ].user() ) { //vats
		score_stage5_  = ScoreFunctionFactory::create_score_function( "score3", option[ OptionKeys::abinitio::stage5_patch ]() );
	} else {
		score_stage5_  = ScoreFunctionFactory::create_score_function( "score3" );
	}


	if ( option[ OptionKeys::abinitio::override_vdw_all_stages ] ) {
		set_score_weight( scoring::vdw, option[ OptionKeys::abinitio::vdw_weight_stage1 ], ALL_STAGES );
	}
}


/// @brief sets a score weight for all stages of abinitio
void ClassicAbinitio::set_score_weight( scoring::ScoreType type, Real setting, StageID stage ) {
	tr.Debug << "set score weights for ";
	if ( stage == ALL_STAGES ) tr.Debug << "all stages ";
	else tr.Debug << "stage " << (stage <= STAGE_3a ? stage : ( stage-1 ) ) << ( stage == STAGE_3b ? "b " : " " );
	tr.Debug << scoring::name_from_score_type(type) << " " << setting << std::endl;
	if ( score_stage1_  && ( stage == STAGE_1  || stage == ALL_STAGES ) ) score_stage1_ ->set_weight(type, setting);
	if ( score_stage2_  && ( stage == STAGE_2  || stage == ALL_STAGES ) ) score_stage2_ ->set_weight(type, setting);
	if ( score_stage3a_ && ( stage == STAGE_3a || stage == ALL_STAGES ) ) score_stage3a_->set_weight(type, setting);
	if ( score_stage3b_ && ( stage == STAGE_3b || stage == ALL_STAGES ) ) score_stage3b_->set_weight(type, setting);
	if ( score_stage4_  && ( stage == STAGE_4  || stage == ALL_STAGES ) ) score_stage4_ ->set_weight(type, setting);
	if ( score_stage4rot_  && ( stage == STAGE_4  || stage == ALL_STAGES ) ) score_stage4rot_ ->set_weight(type, setting);
	if ( score_stage5_  && ( stage == STAGE_5  || stage == ALL_STAGES ) ) score_stage5_ ->set_weight(type, setting);//vats
}

//@brief currently used score function ( depends on stage )
scoring::ScoreFunction const& ClassicAbinitio::current_scorefxn() const {
	return mc().score_function();
}

//@brief set current scorefunction
void ClassicAbinitio::current_scorefxn( scoring::ScoreFunction const& scorefxn ) {
	mc().score_function( scorefxn );
}

//@brief set individual weight of current scorefunction --- does not change the predefined scores: score_stageX_
void ClassicAbinitio::set_current_weight( core::scoring::ScoreType type, core::Real setting ) {
	scoring::ScoreFunctionOP scorefxn ( mc().score_function().clone() );
	scorefxn->set_weight( type, setting );
	mc().score_function( *scorefxn ); //trigger rescore
}

void ClassicAbinitio::set_default_options() {
	bSkipStage1_ = bSkipStage2_ = bSkipStage3_ = bSkipStage4_ = false;
	bSkipStage5_ = true; //vats turned off by default
	using namespace basic::options;
	just_smooth_cycles_ = option[ OptionKeys::abinitio::smooth_cycles_only ]; // defaults to false
	bQuickTest_ = basic::options::option[ basic::options::OptionKeys::run::test_cycles ]();

	if ( bQuickTest() ) {
		set_cycles( 0.001 );
	} else {
		set_cycles( option[ OptionKeys::abinitio::increase_cycles ] ); // defaults to factor of 1.0
	}

	if ( just_smooth_cycles_ ) {
		bSkipStage1_ = bSkipStage2_ = bSkipStage3_ = bSkipStage5_ = true;
	}
	if ( option[ OptionKeys::abinitio::only_stage1 ] ) {
		bSkipStage2_ = bSkipStage3_ = bSkipStage4_ = bSkipStage5_= true;
	}

	if ( option[ OptionKeys::abinitio::include_stage5 ] ) {
		bSkipStage5_ = false;
	}

	apply_large_frags_   = true;  // apply large frags in phase 2!

	// in rosetta++ switched on in fold_abinitio if contig_size < 30 in pose_abinitio never
	short_insert_region_ = false;  // apply small fragments in phase 2!

	if ( option[ OptionKeys::abinitio::recover_low_in_stages ].user() ) {
		for ( int it : option[ OptionKeys::abinitio::recover_low_in_stages ]() ) {
			if ( it == 1 ) recover_low_stages_.push_back( STAGE_1 );
			else if ( it == 2 ) recover_low_stages_.push_back( STAGE_2 );
			else if ( it == 3 ) {
				recover_low_stages_.push_back( STAGE_3a );
				recover_low_stages_.push_back( STAGE_3b );
			} else if ( it == 4 ) recover_low_stages_.push_back( STAGE_4 );
		}
	} else {
		recover_low_stages_.clear();
		recover_low_stages_.push_back( STAGE_1 );
		recover_low_stages_.push_back( STAGE_2 );
		recover_low_stages_.push_back( STAGE_3a );
		recover_low_stages_.push_back( STAGE_3b );
		recover_low_stages_.push_back( STAGE_4 );
		recover_low_stages_.push_back( STAGE_5 );
	}

	close_chbrk_ = option[ OptionKeys::abinitio::close_chbrk ];

}


/// @brief (helper) functor class which keeps track of old pose for the
/// convergence check in stage3 cycles
/// @detail
/// calls of operator ( pose ) compare the
class hConvergenceCheck;
using hConvergenceCheckOP = utility::pointer::shared_ptr<hConvergenceCheck>;

class hConvergenceCheck : public moves::PoseCondition {
public:
	hConvergenceCheck()= default;
	void reset() { ct_ = 0; bInit_ = false; }
	void set_trials( moves::TrialMoverOP trin ) {
		trials_ = trin;
		runtime_assert( trials_->keep_stats_type() < moves::no_stats );
		last_move_ = 0;
	}
	bool operator() ( const core::pose::Pose & pose ) override;
private:
	pose::Pose very_old_pose_;
	bool bInit_{ false };
	Size ct_ = 0;
	moves::TrialMoverOP trials_;
	Size last_move_;
};

// keep going --> return true
bool hConvergenceCheck::operator() ( const core::pose::Pose & pose ) {
	if ( !bInit_ ) {
		bInit_ = true;
		very_old_pose_ = pose;
		return true;
	}
	runtime_assert( trials_ != nullptr );
	tr.Trace << "TrialCounter in hConvergenceCheck: " << trials_->num_accepts() << std::endl;
	if ( numeric::mod(trials_->num_accepts(),100) != 0 ) return true;
	if ( (Size) trials_->num_accepts() <= last_move_ ) return true;
	last_move_ = trials_->num_accepts();
	// change this later to this: (after we compared with rosetta++ and are happy)
	// if ( numeric::mod(++ct_, 1000) != 0 ) return false; //assumes an approx acceptance rate of 0.1

	// still here? do the check:

	core::Real converge_rms = core::scoring::CA_rmsd( very_old_pose_, pose );
	very_old_pose_ = pose;
	if ( converge_rms >= 3.0 ) {
		return true;
	}
	// if we get here thing is converged stop the While-Loop
	tr.Info << " stop cycles in stage3 due to convergence " << std::endl;
	return false;
}


bool ClassicAbinitio::do_stage1_cycles( pose::Pose &pose ) {
	AllResiduesChanged done( pose, brute_move_large()->insert_map(), *movemap() );
	moves::MoverOP trial( stage1_mover( pose, trial_large() ) );

	// FragmentMoverOP frag_mover = brute_move_large_;
	// fragment::FragmentIO().write("stage1_frags_classic.dat",*frag_mover->fragments());

	Size j;
	for ( j = 1; j <= stage1_cycles(); ++j ) {
		trial->apply( pose ); // apply a large fragment insertion, accept with MC boltzmann probability
		if ( done(pose) ) {
			tr.Info << "Replaced extended chain after " << j << " cycles." << std::endl;
			mc().reset( pose ); // make sure that we keep the final structure
			return true;
		}
	}
	tr.Warning << "extended chain may still remain after " << stage1_cycles() << " cycles!" << std::endl;
	done.show_unmoved( pose, tr.Warning );
	mc().reset( pose ); // make sure that we keep the final structure
	return true;
}

bool ClassicAbinitio::do_stage2_cycles( pose::Pose &pose ) {

	//setup cycle
	moves::SequenceMoverOP cycle( new moves::SequenceMover() );
	if ( apply_large_frags_   ) cycle->add_mover( trial_large_->mover() );
	if ( short_insert_region_ ) cycle->add_mover( trial_small_->mover() );

	Size nr_cycles = stage2_cycles() / ( short_insert_region_ ? 2 : 1 );
	moves::TrialMoverOP trials( new moves::TrialMover( cycle, mc_ptr() ) );
	moves::RepeatMover( stage2_mover( pose, trials ), nr_cycles ).apply(pose);

	//is there a better way to find out how many steps ? for instance how many calls to scoring?
	return true; // as best guess
}

/*! @detail stage3 cycles:
nloop1 : outer iterations
nloop2 : inner iterations
stage3_cycle : trials per inner iteration
every inner iteration we switch between score_stage3a ( default: score2 ) and score_stage3b ( default: score 5 )

prepare_loop_in_stage3() is called before the stage3_cycles() of trials are started.

first outer loop-iteration is done with TrialMover trial_large()
all following iterations with trial_small()

start each iteration with the lowest_score_pose. ( mc->recover_low() -- called in prepare_loop_in_stage3() )

*/
bool ClassicAbinitio::do_stage3_cycles( pose::Pose &pose ) {
	using namespace ObjexxFCL;

	// interlaced score2 / score 5 loops
	// nloops1 and nloops2 could become member-variables and thus changeable from the outside
	int nloop1 = 1;
	int nloop2 = 10; //careful: if you change these the number of structures in the structure store changes.. problem with checkpointing
	// individual checkpoints for each stage3 iteration would be a remedy. ...

	if ( short_insert_region_ ) {
		nloop1 = 2;
		nloop2 = 5;
	}

	hConvergenceCheckOP convergence_checker ( nullptr );
	if ( !option[ basic::options::OptionKeys::abinitio::skip_convergence_check ] ) {
		convergence_checker = hConvergenceCheckOP( new hConvergenceCheck );
	}

	moves::TrialMoverOP trials = trial_large();
	int iteration = 1;
	for ( int lct1 = 1; lct1 <= nloop1; lct1++ ) {
		if ( lct1 > 1 ) trials = trial_small(); //only with short_insert_region!
		for ( int lct2 = 1; lct2 <= nloop2; lct2++, iteration++  ) {
			tr.Debug << "Loop: " << lct1 << "   " << lct2 << std::endl;

			if ( !prepare_loop_in_stage3( pose, iteration, nloop1*nloop2 ) ) return false;

			if ( !get_checkpoints().recover_checkpoint( pose, get_current_tag(), "stage_3_iter"+string_of( lct1)+"_"+string_of(lct2),
					false /*fullatom */, true /*fold tree */ ) ) {


				tr.Debug << "  Score stage3 loop iteration " << lct1 << " " << lct2 << std::endl;
				if ( convergence_checker ) {
					moves::TrialMoverOP stage3_trials = stage3_mover( pose, lct1, lct2, trials );
					convergence_checker->set_trials( stage3_trials ); //can be removed late
					moves::WhileMover( stage3_trials, stage3_cycles(), convergence_checker ).apply( pose );
				} else {    //no convergence check -> no WhileMover
					moves::RepeatMover( stage3_mover( pose, lct1, lct2, trials ), stage3_cycles() ).apply( pose );
				}

				if ( numeric::mod( (int)iteration, 2 ) == 0 || iteration > 7 ) recover_low( pose, STAGE_3a );
				recover_low( pose, STAGE_3b );

				get_checkpoints().checkpoint( pose, get_current_tag(), "stage_3_iter"+string_of( lct1)+"_"+string_of(lct2), true /*fold tree */ );
			}//recover_checkpoint
			get_checkpoints().debug( get_current_tag(), "stage_3_iter"+string_of( lct1)+"_"+string_of(lct2), current_scorefxn()( pose ) );

			//   structure_store().push_back( mc_->lowest_score_pose() );
		} // loop 2
	} // loop 1
	return true;
}


// interlaced score2 / score 5 loops
/*! @detail stage4 cycles:
nloop_stage4: iterations
stage4_cycle : trials per  iteration

first iteration: use trial_small()
following iterations: use trial_smooth()
only trial_smooth() if just_smooth_cycles==true

prepare_loop_in_stage4() is called each time before the stage4_cycles_ of trials are started.

start each iteration with the lowest_score_pose. ( mc->recover_low()  in prepare_loop_in_stage4()  )

*/
bool ClassicAbinitio::do_stage4_cycles( pose::Pose &pose ) {
	Size nloop_stage4 = 3;

	using namespace basic::options;
	using namespace basic::options::OptionKeys;

	if ( option[corrections::score::cenrot]() ) nloop_stage4=2;

	for ( Size kk = 1; kk <= nloop_stage4; ++kk ) {
		tr.Debug << "prepare ..." << std::endl ;
		if ( !prepare_loop_in_stage4( pose, kk, nloop_stage4 ) ) return false;

		if ( !get_checkpoints().recover_checkpoint( pose, get_current_tag(), "stage4_kk_" + ObjexxFCL::string_of(kk), false /* fullatom */, true /* fold_tree */ ) ) {
			moves::TrialMoverOP trials;
			if ( kk == 1 && !just_smooth_cycles_ ) {
				trials = trial_small();
			} else {
				tr.Debug << "switch to smooth moves" << std::endl;
				trials = trial_smooth();
			}

			tr.Debug << "start " << stage4_cycles() << " cycles" << std::endl;
			moves::RepeatMover( stage4_mover( pose, kk, trials ), stage4_cycles() ).apply(pose);
			tr.Debug << "finished" << std::endl;
			recover_low( pose, STAGE_4 );

			get_checkpoints().checkpoint( pose, get_current_tag(), "stage4_kk_" + ObjexxFCL::string_of(kk), true /*fold tree */ );
		}
		get_checkpoints().debug( get_current_tag(), "stage4_kk_" + ObjexxFCL::string_of(kk),  current_scorefxn()( pose ) );

		//don't store last structure since it will be exactly the same as the final structure delivered back via apply
		//  if( kk < nloop_stage4 ) // <-- this line was missing although the comment above was existant.
		//   structure_store().push_back( mc_->lowest_score_pose() );
	}  // loop kk

	if ( option[corrections::score::cenrot] ) {
		//switch to cenrot model
		tr.Debug << "switching to cenrot model ..." << std::endl;
		protocols::simple_moves::SwitchResidueTypeSetMover to_cenrot(chemical::CENTROID_ROT);
		to_cenrot.apply(pose);

		//init pose
		(*score_stage4rot_)( pose );
		pack_rotamers_->score_function(score_stage4rot_sc_);
		pack_rotamers_->apply(pose);

		mc_->reset(pose);
		replace_scorefxn( pose, STAGE_4rot, 0 );
		//mc_->set_temperature(1.0);
		//mc_->set_autotemp(true, 1.0);

		//debug
		//tr.Debug << "starting_energy: " << (*score_stage4rot_)( pose ) << std::endl;
		//tr.Debug << "starting_temperature: " << mc_->temperature() << std::endl;

		for ( Size rloop=1; rloop<=3; rloop++ ) {
			//change vdw weight
			switch (rloop) {
			case 1 :
				score_stage4rot_->set_weight(core::scoring::vdw, score_stage4rot_->get_weight(core::scoring::vdw)/9.0);
				break;
			case 2 :
				score_stage4rot_->set_weight(core::scoring::vdw, score_stage4rot_->get_weight(core::scoring::vdw)*3.0);
				break;
			case 3 :
				score_stage4rot_->set_weight(core::scoring::vdw, score_stage4rot_->get_weight(core::scoring::vdw)*3.0);
				break;
			}

			//stage4rot
			//for (Size iii=1; iii<=100; iii++){
			//pose::Pose startP = pose;
			//tr << "temperature: " << mc_->temperature() << std::endl;
			moves::RepeatMover( stage4rot_mover( pose, rloop, trial_smooth() ), stage4_cycles()/100 ).apply(pose);
			//tr << "delta_rms: " << core::scoring::CA_rmsd( startP, pose ) << std::endl;
			//}
		}
	}

	return true;
}

bool ClassicAbinitio::do_stage5_cycles( pose::Pose &pose ) {//vats

	Size nmoves = 1;
	core::kinematics::MoveMapOP mm_temp( new core::kinematics::MoveMap( *movemap() ) );
	simple_moves::SmallMoverOP small_mover( new simple_moves::SmallMover( mm_temp, temperature_, nmoves) );
	small_mover->angle_max( 'H', 2.0 );
	small_mover->angle_max( 'E', 2.0 );
	small_mover->angle_max( 'L', 5.0 );

	moves::TrialMoverOP trials( new moves::TrialMover( small_mover, mc_ptr() ) );
	moves::RepeatMover( stage5_mover( pose, trials ), stage5_cycles() ).apply( pose );

	// moves::MoverOP trial( stage5_mover( pose, small_mover ) );
	// Size j;
	// for( j = 1; j <= stage5_cycles(); ++j ) {
	//  trial->apply( pose );
	// }
	mc().reset( pose );
	return true;

}


moves::TrialMoverOP
ClassicAbinitio::stage1_mover( pose::Pose &, moves::TrialMoverOP trials ) {
	return trials;
}


moves::TrialMoverOP
ClassicAbinitio::stage2_mover( pose::Pose &, moves::TrialMoverOP trials ) {
	return trials;
}


moves::TrialMoverOP
ClassicAbinitio::stage3_mover( pose::Pose &, int, int, moves::TrialMoverOP trials ) {
	return trials;
}

moves::TrialMoverOP
ClassicAbinitio::stage4_mover( pose::Pose &, int, moves::TrialMoverOP trials ) {
	return trials;
}

moves::TrialMoverOP
ClassicAbinitio::stage4rot_mover( pose::Pose &, int, moves::TrialMoverOP trials ) {
	if ( trials == trial_small_ ) {
		return trial_small_pack_;
	} else {
		return smooth_trial_small_pack_;
	}
}

moves::TrialMoverOP //vats
ClassicAbinitio::stage5_mover( pose::Pose &, moves::TrialMoverOP trials ) {
	return trials;
}

void ClassicAbinitio::recover_low( core::pose::Pose& pose, StageID stage ){
	if ( contains_stageid( recover_low_stages_, stage ) ) {
		mc_->recover_low( pose );
	}
}

// anything you want to have done before the stages ?
void ClassicAbinitio::replace_scorefxn( core::pose::Pose& pose, StageID stage, core::Real /*intra_stage_progress */ ) {
	// must assume that the current pose is the one to be accepted into the next stage! (this change was necessary for
	// checkpointing to work correctly.

	//intra_stage_progress = intra_stage_progress;
	if ( score_stage1_  && ( stage == STAGE_1 ) ) current_scorefxn( *score_stage1_ );
	if ( score_stage2_  && ( stage == STAGE_2 ) ) current_scorefxn( *score_stage2_ );
	if ( score_stage3a_ && ( stage == STAGE_3a) ) current_scorefxn( *score_stage3a_ );
	if ( score_stage3b_ && ( stage == STAGE_3b) ) current_scorefxn( *score_stage3b_ );
	if ( score_stage4_  && ( stage == STAGE_4 ) ) current_scorefxn( *score_stage4_ );
	if ( score_stage4rot_  && ( stage == STAGE_4rot ) ) current_scorefxn( *score_stage4rot_ );
	if ( score_stage5_  && ( stage == STAGE_5 ) ) current_scorefxn( *score_stage5_ );//vats
	Real temperature( temperature_ );
	if ( stage == STAGE_5 ) temperature = 0.5;
	mc_->set_autotemp( true, temperature );
	mc_->set_temperature( temperature ); // temperature might have changed due to autotemp..
	mc_->reset( pose );
}


moves::TrialMoverOP ClassicAbinitio::trial_large() {
	return ( apply_large_frags_ ? trial_large_ : trial_small_ );
}

moves::TrialMoverOP ClassicAbinitio::trial_small() {
	return trial_small_;
}

moves::TrialMoverOP ClassicAbinitio::trial_smooth() {
	return smooth_trial_small_;
}

// prepare stage1 sampling
bool ClassicAbinitio::prepare_stage1( core::pose::Pose &pose ) {
	replace_scorefxn( pose, STAGE_1, 0.5 );
	mc_->set_autotemp( false, temperature_ );
	// mc_->set_temperature( temperature_ ); already done in replace_scorefxn
	// mc_->reset( pose );
	(*score_stage1_)( pose );
	/// Now handled automatically.  score_stage1_->accumulate_residue_total_energies( pose ); // fix this
	return true;
}

bool ClassicAbinitio::prepare_stage2( core::pose::Pose &pose ) {
	replace_scorefxn( pose, STAGE_2, 0.5 );

	(*score_stage2_)(pose);
	/// Now handled automatically.  score_stage2_->accumulate_residue_total_energies( pose );
	return true;
}


bool ClassicAbinitio::prepare_stage3( core::pose::Pose &pose ) {
	replace_scorefxn( pose, STAGE_3a, 0 );
	//score for this stage is changed in the do_stage3_cycles explicitly
	if ( option[ templates::change_movemap ].user() && option[ templates::change_movemap ] == 3 ) {
		kinematics::MoveMapOP new_mm( new kinematics::MoveMap( *movemap() ) );
		new_mm->set_bb( true );
		set_movemap( new_mm ); // --> store it in movemap_ --> original will be reinstated at end of apply()
	}
	return true;
}


bool ClassicAbinitio::prepare_stage4( core::pose::Pose &pose ) {
	replace_scorefxn( pose, STAGE_4, 0 );
	(*score_stage4_)( pose );
	/// Now handled automatically.  score_stage4_->accumulate_residue_total_energies( pose ); // fix this

	if ( option[ templates::change_movemap ].user() && option[ templates::change_movemap ] == 4 ) {
		kinematics::MoveMapOP new_mm( new kinematics::MoveMap( *movemap() ) );
		new_mm->set_bb( true );
		tr.Debug << "option: templates::change_movemap ACTIVE: set_movemap" << std::endl;
		set_movemap( new_mm ); // --> store it in movemap_ --> original will be reinstated at end of apply()
	}
	return true;
}

bool ClassicAbinitio::prepare_stage5( core::pose::Pose &pose ) {//vats
	// temperature_ = 0.5; //this has to be reset to original temperature!!!
	// no special if-statement in replace_scorefxn...OL
	replace_scorefxn( pose, STAGE_5, 0 );
	(*score_stage5_)( pose );
	return true;
}


bool ClassicAbinitio::prepare_loop_in_stage3( core::pose::Pose &pose/*pose*/, Size iteration, Size total ){
	// interlace score2/score5

	Real chbrk_weight_stage_3a = 0;
	Real chbrk_weight_stage_3b = 0;

	if ( numeric::mod( (int)iteration, 2 ) == 0 || iteration > 7 ) {
		Real progress( iteration );
		chbrk_weight_stage_3a = 0.25 * progress;
		tr.Debug << "select score_stage3a..." << std::endl;
		recover_low( pose, STAGE_3a );
		replace_scorefxn( pose, STAGE_3a, 1.0* iteration/total );
	} else {
		Real progress( iteration );
		chbrk_weight_stage_3b = 0.05 * progress;
		tr.Debug << "select score_stage3b..." << std::endl;
		recover_low( pose, STAGE_3b );
		replace_scorefxn( pose, STAGE_3b, 1.0* iteration/total );
	}

	if ( close_chbrk_ ) {

		set_score_weight( scoring::linear_chainbreak, chbrk_weight_stage_3a , STAGE_3a );
		set_score_weight( scoring::linear_chainbreak, chbrk_weight_stage_3b , STAGE_3b );

	}


	return true;
}

bool ClassicAbinitio::prepare_loop_in_stage4( core::pose::Pose &pose, Size iteration, Size total ){
	replace_scorefxn( pose, STAGE_4, 1.0* iteration/total );

	Real chbrk_weight_stage_4 (iteration*0.5+2.5);

	if ( close_chbrk_ ) {
		set_current_weight( scoring::linear_chainbreak, chbrk_weight_stage_4 );
	}

	return true;
}

//@brief obtain currently used monte-carlo object --> use to obtain current score-func: mc().score_function()
moves::MonteCarloOP
ClassicAbinitio::mc_ptr() {
	return mc_;
}


void ClassicAbinitio::output_debug_structure( core::pose::Pose & pose, std::string prefix ) {
	using namespace core::io::silent;

	mc().score_function()( pose );
	Parent::output_debug_structure( pose, prefix );

	if ( option[ basic::options::OptionKeys::abinitio::explicit_pdb_debug ]() ) {
		pose.dump_pdb( prefix + get_current_tag() + ".pdb" );
	}

	if ( option[ basic::options::OptionKeys::abinitio::log_frags ].user() ) {
		std::string filename = prefix + "_" + get_current_tag() + "_" + std::string( option[ basic::options::OptionKeys::abinitio::log_frags ]() );
		utility::io::ozstream output( filename );
		auto& log_frag = dynamic_cast< simple_moves::LoggedFragmentMover& > (*brute_move_large_);
		log_frag.show( output );
		log_frag.clear();
	}

} // ClassicAbinitio::output_debug_structure( core::pose::Pose & pose, std::string prefix )

} //abinitio
} //protocols
