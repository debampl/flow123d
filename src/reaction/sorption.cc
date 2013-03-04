#include <iostream>
#include <cstring>
#include <stdlib.h>
#include <math.h>
#include <boost/foreach.hpp>

#include "reaction/reaction.hh"
#include "reaction/linear_reaction.hh"
#include "reaction/pade_approximant.hh"
#include "reaction/sorption.hh"
#include "system/system.hh"
#include "system/sys_profiler.hh"

#include "la/distribution.hh"
#include "mesh/mesh.h"
#include "transport/transport.h" //because of definition of constants MOBILE, IMMOBILE,

namespace it=Input::Type;

Sorption::EqData::EqData(const std::string &name)
: EqDataBase(name)
{
    //ADD_FIELD(nr_of_points, "Number of crossections allong isotherm", Input::Type::Default("10"));
    //ADD_FIELD(region_ident, "Rock matrix area identifier.", Input::Type::Default("0"));
    ADD_FIELD(rock_density, "Rock matrix density.", Input::Type::Default("0.0"));
    //ADD_FIELD(solvent_density, "Solvent density.", Input::Type::Default("1.0"));

    ADD_FIELD(sorption_types,"Considered adsorption is described by selected isotherm.", it::Default("none") );
              sorption_types.set_selection(&sorption_type_selection);

    ADD_FIELD(mult_coefs,"Multiplication parameters (k, omega) in either Langmuir c_s = omega * (alpha*c_a)/(1- alpha*c_a) or in linear c_s = k * c_a isothermal description.");
    //std::vector<FieldEnum> list; list.push_back(none); list.push_back(Langmuir); list.push_back(Freundlich);
    //slopes.disable_where(& sorption_type, list );

    //ADD_FIELD(omegas,"Langmuir isotherm multiplication parameters in c_s = omega * (alpha*c_a)/(1- alpha*c_a).");
    //list.clear(); list.push_back(none); list.push_back(Linear); list.push_back(Freundlich);
    //omegas.disable_where(& sorption_type, list );

    ADD_FIELD(alphas,"Second parameters (alpha, ...) defining isotherm  c_s = omega * (alpha*c_a)/(1- alpha*c_a).");
    //list.clear(); list.push_back(none); list.push_back(Linear); list.push_back(Freundlich);
    //alphas.disable_where(& sorption_type, list );
    ADD_FIELD(mob_porosity,"Mobile porosity of the rock matrix.");
    ADD_FIELD(immob_porosity,"Immobile porosity of the rock matrix.", Input::Type::Default("0.0"));
    //ADD_FIELD(specie,"Names of species undergiong sorption.", Input::Type::Default("0.0"));
}

/*RegionSet Sorption::EqData::read_bulk_list_item(Input::Record rec) {
    RegionSet domain=EqDataBase::read_bulk_list_item(rec);
    Input::AbstractRecord field_a_rec;
    if (rec.opt_val("init_piezo_head", field_a_rec)) {
                init_pressure.set_field(domain, boost::make_shared< FieldAddPotential<3, FieldValue<3>::Scalar > >( this->gravity_, field_a_rec) );
    }
    return domain;
}*/

using namespace Input::Type;

/*Record Sorption::input_type_isotherm // region independent parameters
	= Record("Isotherm", "Equation for reading information about limmited solubility affected sorption.")
	.declare_key("specie", String(), Default::obligatory(),
				"Identifier of a sorbing isotope.")
	.declare_key("molar_mass", Double(), Default("1.0"),
				"Molar mass.")
	.declare_key("solvable", Double(), Default("1.0"),   // concentration limit for a solubility of the specie under concideration
				"Solubility limit.");*/

Record Sorption::input_type
	= Record("Sorptions", "Information about all the limited solubility affected sorptions.")
	.derive_from( Reaction::input_type )
	.declare_key("water_dens", Double(), Default("1.0"),
				"Density of the solvent.")
	.declare_key("substeps", Integer(), Default("10"),
				"Number of equidistant substeps, molar mass and isotherm intersections")
	.declare_key("species", Array(String()), Default::obligatory(),
							"Names of all the sorbing species")
	.declare_key("molar_mass", Array(Double()), Default::obligatory(),
							"Specifies molar masses of all the sorbing species")
	.declare_key("solubility", Array(Double()), Default::obligatory(),
							"Specifies solubility limits of all the sorbing species")
    /*.declare_key("sorptions", Array( Sorption::input_type_isotherm ), Default::obligatory(),
                "Description of particular sorption cases under consideration.")*/
    .declare_key("bulk_data", Array(Sorption::EqData().bulk_input_type()), Default::obligatory(),
                   	   	   "Containes region specific data necessery to construct isotherms.");

using namespace std;

Sorption::Sorption(Mesh &init_mesh, Input::Record in_rec, vector<string> &names)
	: Reaction(init_mesh, in_rec, names)
{
	nr_of_regions = init_mesh.n_materials;
	nr_of_substances = names.size();

	TimeGovernor tg(0.0, 1.0);

    //data.init_conc.set_n_comp(4);        // set number of substances posibly read from elsewhere
    //data.bc_conc.set_n_comp(4);

    data_.set_mesh(&init_mesh);
    data_.init_from_input( in_rec.val<Input::Array>("bulk_data"),Input::Array() );
    data_.set_time(tg);
	//both following operations connected together in compute_isotherm(..) method
	//prepare_inputs(in_rec);
	//determine_crossection()
	//compute_isotherms(in_rec);
}

void Sorption::prepare_inputs(Input::Record in_rec)
{
    unsigned int idx;

	Input::Array sorption_array = in_rec.val<Input::Array>("sorptions");

	//Simple vectors holding  common informations.
	region_ids.resize( sorption_array.size() ); // ( nr_of_regions );
	substance_ids.resize(sorption_array.size()); // ( nr_of_substances );
	molar_masses.resize( nr_of_substances );
	c_aq_max.resize( nr_of_substances );
	//Multidimensional array angle, initialization
	angle.resize(nr_of_regions*nr_of_substances); //|nr_of_region x nr_of_substances| doubles. Radians.
	//for(int i_mob = 0; i_mob < 2; i_mob++)
	//{
		//angle[i_mob].resize(nr_of_regions);
		for(int i_reg = 0; i_reg < nr_of_regions; i_reg++)
		{
			//angle[i_mob][i_reg].resize(nr_of_substances);
			for(int i_subst = 0; i_subst < nr_of_substances; i_subst++)
			{
				angle[i_reg][i_subst] = 0.0;
			}
		}
	//}
	//Multidimensional array isotherm, initialization
	isotherm.resize(nr_of_regions*nr_of_substances*nr_of_points); //Up to |nr_of_region x nr_of_substances x n_points| doubles.
	for(int i_reg = 0; i_reg < nr_of_regions; i_reg++)
	{
		//isotherm[i_mob].resize(nr_of_regions);
		for(int i_subst = 0; i_subst < nr_of_substances; i_subst++)
		{
			//angle[i_mob][i_reg].resize(nr_of_substances);
			for(int i_point = 0; i_point < nr_of_points; i_point++)
			{
				isotherm[i_reg][i_subst][i_point] = 0;
			}
		}
	}

	int i_sorp = 0;
	for (Input::Iterator<Input::Record> sorp_it = sorption_array.begin<Input::Record>(); sorp_it != sorption_array.end(); ++sorp_it, ++i_sorp)
	{
		int idx;
		double mol_mass, solub;

		//indices determining part
		string specie_name = sorp_it->val<string>("specie");
		idx = find_subst_name(specie_name);
		if (idx < n_substances())
		{
			substance_ids[i_sorp] = idx;
		}else{
			//xprintf(UsrErr,"Unknown name %s of substance undergoing the %d-th sorption.\n", specie_name, i_sorp);
			xprintf(UsrErr,"Unknown name (identifier) of the substance undergoing the %d-th sorption.\n", i_sorp);
		}

		// solubulity limit
		solub = sorp_it->val<double>("solubility");
		if (solub > 0.0) {
		   c_aq_max[idx] = solub;
		} else {
			//xprintf(UsrErr, "Unknown solubility limit of substance %s undergoing the %d-th sorption.\n", specie_name, i_sorp);
			xprintf(UsrErr, "Unknown solubility limit of the substance undergoing the %d-th sorption.\n", i_sorp);
		}

		//molar masses determining part
		mol_mass = sorp_it->val<double>("molar_mass");
		if (mol_mass) {
		   molar_masses[idx] = mol_mass;
		} else {
			//xprintf(UsrErr, "Unknown molar mass of substance %s undergoing the %d-th sorption.\n", specie_name, i_sorp);
			xprintf(UsrErr, "Unknown molar mass of the substance undergoing the %d-th sorption.\n", i_sorp);
		}

		//region determining part
		idx = sorp_it->val<int>("region");
		if(idx)
		{
			region_ids[i_sorp] = idx;
		}else{
			xprintf(UsrErr, "Undefined region identifier where the %d-th sorption takes place.\n", i_sorp);
		}

	}

}

void Sorption::compute_angles(void)
{
	; //Compute coordinate system rotation angles from region specific data.
}

void Sorption::precompute_isotherm_tables() {
    BOOST_FOREACH(const Region & reg, this->mesh_->region_db().get_region_set("BULK")) {
        arma::Col<unsigned int> sorption_type_vec;
        arma::Col<double> scale_vec, alpha_vec;
        if (data_.sorption_types.get_const_value(reg, sorption_type_vec))
        if (data_.mult_coefs.get_const_value(reg, scale_vec))
        if (data_.alphas.get_const_value(reg, alpha_vec)) {
            // precompute isotherm
        }
        // else leave isotherm empty for this region
    }
}

// TODO: check duplicity of parents
//       raise warning if sum of ratios is not one
void Sorption::compute_isotherms(Input::Record in_rec)
{
    unsigned int idx;

	Input::Array sorption_array = in_rec.val<Input::Array>("sorptions");
	/*

	isotherms.resize(nr_of_regions*nr_of_substances);
	for(int i = 0; i < nr_of_regions; i++)
	{
		for(int j = 0; j < nr_of_substances; j++)
		{
			isotherms[i][j] = 0; //initialization
		}
	}

	int i_sorption=0;
	for (Input::Iterator<Input::Record> sorp_it = sorption_array.begin<Input::Record>(); sorp_it != sorption_array.end(); ++sorp_it, ++i_sorption)
	{
		//isotherm determining part
		n_points = in_rec.val<int>("substeps");
		int region_id = sorp_it->find<int>("region");
		int substance_id = sorp_it->find<int>("specie");
		isotherms[region_id][specie_id] = new Isotherm(init_mesh, *sorp_it, names);
		(&isotherms[region_id][specie_id])->set_parameters(*sorp_it);
		determine_crossections(n_points);//
		rotate_points(0.70711, isotherms[region_id][substance_id]);*/

		//Sorption_type sorption_type = sorp_it->find<enum Sorption_type>("type");
		/*double slope = sorp_it->find<double>("direction");
		if (slope > 0.0) {
		   ; //intersections of an isotherm with mass balance lines can be found exactly
		} else {
		   double alpha = sorp_it->find<double>("alpha");
		   double omega = sorp_it->find<double>("omega");
		   if ((alpha == -1.0) || (omega == -1.0)) {
			   xprintf(UsrErr, "Some of parameters for isotherm are either missing or incorect.\n");
		   } else {
			   int substance_id = sorp_it->find<int>("specie");
			   int region_id = sorp_it->find<int>("region");
		  }
		}*/

		//isotherm type determining part
		/*string parent_name = sorp_it->val<string>("type");
		Input::Array product_array = sorp_it->val<Input::Array>("products");
		Input::Array ratio_array = sorp_it->val<Input::Array>("branch_ratios"); // has default value [ 1.0 ]*/

		// linear isotherm direction (slope)
		/*if (product_array.size() > 0)   substance_ids[i_sorption].resize( product_array.size()+1 );
		else			xprintf(UsrErr,"Empty array of products in the %d-th reaction.\n", i_sorption);*/


		// multiplicative coefficient omega for Langmuir isotherm, c_s = omega*(alpha*c_a)/(1 - alpha*c_a)
		/*idx = find_subst_name(parent_name);
		if (idx < n_substances())	substance_ids[i_sorption][0] = idx;
		else                		xprintf(UsrErr,"Wrong name of parent substance in the %d-th reaction.\n", i_sorption);*/

		// alpha parameter for Langmuir isotherm, c_s = omega*(alpha*c_a)/(1 - alpha*c_a)
		/*unsigned int i_product = 1;
		for(Input::Iterator<string> product_it = product_array.begin<string>(); product_it != product_array.end(); ++product_it, i_product++)
		{
			idx = find_subst_name(*product_it);
			if (idx < n_substances())   substance_ids[i_sorption][i_product] = idx;
			else                    	xprintf(Msg,"Wrong name of %d-th product in the %d-th reaction.\n", i_product-1 , i_sorption);
		}*/

		//Critical concentrations solvable in water.
        /*if (ratio_array.size() == product_array.size() )   ratio_array.copy_to( bifurcation[i_sorption] );
        else            xprintf(UsrErr,"Number of branches %d has to match number of products %d in the %d-th reaction.\n",
                                       ratio_array.size(), product_array.size(), i_sorption);*/

		// Molar mass of particular adsorbent.

	//}
}

double **Sorption::compute_reaction(double **concentrations, int loc_el) // Sorptions are realized just for one element.
{
    //int cols, rows;
    double porosity;
    int reg_id;
    //std::vector<std::vector<double> > previous_conc; //to backup either {MOBILE, MOBILE_SORB} or {IMMOBILE, IMMOBILE_SORB} concentrations
    std::vector<double> previous_conc; //to backup either {MOBILE, MOBILE_SORB} or {IMMOBILE, IMMOBILE_SORB} concentrations
    previous_conc.resize(2);

    //	Identify loc_el region.
    //  If intersections of isotherm with mass balance lines are known, then interpolate.
    	//  Measurements [c_a,c_s] will be rotated
    	//  Rotated measurements must be projected on rotated isotherm, interpolate_datapoints()
    	//  Projections need to be transformed back to original CS
    //	If intersections are not known then solve the problem analytically (toms748_solve).

    //*if (reaction_matrix == NULL) return concentrations;

    porosity = 0.1; //must be changed using accesors

	std::vector<double> rot_point;
	rot_point.resize(2);

    for(int i_subst = 0; i_subst < n_substances(); i_subst++){

		previous_conc[0] = concentration_matrix[MOBILE][i_subst][loc_el];
		//concentration_matrix[MOBILE][i_subst][loc_el] = 0.0;
		previous_conc[1] = concentration_matrix[MOBILE_SORB][i_subst][loc_el];
		//concentration_matrix[MOBILE_SORB][i_subst][loc_el] = 0.0;

		rot_point = rotate_point(angle[reg_id][i_subst], previous_conc); //counterclockwise rotation to mass balancing coordination system
		rot_point[2] = interpolate_datapoint(rot_point, reg_id, i_subst); // interpolation in mass ballancing coordination system
		previous_conc = rotate_point((-1.0)*angle[reg_id][i_subst], rot_point); //clockwise rotation back to original coodinate system
		concentration_matrix[MOBILE][i_subst][loc_el] = previous_conc[0];
		concentration_matrix[MOBILE_SORB][i_subst][loc_el] = previous_conc[1];

		if(dual_porosity_on)
		{
			//The same as above repeated for immobile pores
			previous_conc[0] = concentration_matrix[IMMOBILE][i_subst][loc_el];
			previous_conc[1] = concentration_matrix[IMMOBILE_SORB][i_subst][loc_el];

			rot_point = rotate_point(angle[reg_id][i_subst], previous_conc); //counterclockwise rotation to mass balancing coordination system
			rot_point[2] = interpolate_datapoint(rot_point, reg_id, i_subst); // interpolation in mass ballancing coordination system
			previous_conc = rotate_point((-1.0)*angle[reg_id][i_subst], rot_point); //clockwise rotation back to original coodinate system
			concentration_matrix[IMMOBILE][i_subst][loc_el] = previous_conc[0];
			concentration_matrix[IMMOBILE_SORB][i_subst][loc_el] = previous_conc[1];
		}
	}

	return concentrations;
}

void Sorption::compute_one_step(void) // Computes sorption simulation over all the elements.
{
    START_TIMER("sorption_step");
	for (int loc_el = 0; loc_el < distribution->lsize(); loc_el++)
	 {
	 	this->compute_reaction(concentration_matrix[0], loc_el); //MOBILE and IMMOBILE 	computed
	    if (dual_porosity_on == true) {
	     this->compute_reaction(concentration_matrix[1], loc_el); //IMMOBILE
	    }

	 }
    END_TIMER("sorption_step");
	 return;
}


void Sorption::print_sorption_parameters(void)
{

    xprintf(Msg, "\nSorption parameters are defined as:");
    /*for (int i = 0; i < (nr_of_substances - 1); i++) {
        if (i < (nr_of_substances - 2)) ; //cout << " " << half_lives[i] <<",";
        	// describing table header
        	// name(specie)
        	// molar mass
        	// isotherm type
        	// region
        	// parameter(s)
        	// solubility limit
            //xprintf(Msg, " %f", half_lives[i]);
        if (i == (nr_of_substances - 2)) ; //cout << " " << half_lives[i] <<"\n";
            // xprintf(Msg, " %f\n", this->half_lives[i]);
    }*/
}

void Sorption::determine_crossections(void)
{
	;
}

std::vector<double> Sorption::rotate_point(double angle, std::vector<double> point)
{
	std::vector<double> rot_point;

	rot_point[1] = cos(angle)*point[1] - sin(angle)*point[2];
	rot_point[2] = sin(angle)*point[1] + cos(angle)*point[2];

	return rot_point;
}

double Sorption::interpolate_datapoint(std::vector<double> rot_point, int region, int specie)
{
	int iso_ind_floor, iso_ind_ceil;
	double interp_val;

	if((rot_point.size()) > 2) xprintf(UsrErr, "Just two coordinates are expected as a parameter for the function interpolate_datapoints(rot_point).\n");
	iso_ind_floor = (int)(rot_point[1]/(step_length)); iso_ind_ceil = iso_ind_floor + 1;
	interp_val = isotherm[region][specie][iso_ind_floor] + (rot_point[1] - isotherm[region][specie][iso_ind_floor])*(isotherm[region][specie][iso_ind_ceil] - isotherm[region][specie][iso_ind_floor])/step_length;

	return interp_val;
}

double Sorption::set_step_length(void)
{
	;
}
