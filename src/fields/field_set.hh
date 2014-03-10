/*
 * field_set.hh
 *
 *  Created on: Mar 8, 2014
 *      Author: jb
 */

#ifndef FIELD_SET_HH_
#define FIELD_SET_HH_


#include <system/exceptions.hh>
#include <fields/field.hh>



/**
 * TODO: implementation robust against destroying fields before the FieldSet.
 */
class FieldSet {
public:
	DECLARE_EXCEPTION(ExcUnknownField, << "Field set has no field with name: " << FieldCommonBase::EI_Field::qval);

	/**
	 * Add an existing Field to the list. It stores just pointer to the field.
	 * Be careful to not destroy passed Field before the FieldSet.
	 *
	 * Using operator allows elegant setting and adding of a field to the field set:
	 * @code
	 * 		Field<...> init_quantity; // member of a FieldSet descendant
	 *
	 * 		field_set +=
	 * 			some_field
	 * 			.disable_where(type, {dirichlet, neumann}); // this must come first since it is not member of FieldCommonBase
	 * 			.name("init_temperature")
	 * 			.description("Initial temperature");
	 *
	 */
	FieldCommonBase &operator +=(FieldCommonBase &field) {
		field_list.push_back(&field);
		return field;
	}

	/**
	 * Make new FieldSet as a subset of *this. The new FieldSet contains fields with names given by the @p names parameter.
	 */
	FieldSet subset(std::vector<std::string> names) const {
		FieldSet set;
		for(auto name : names) set += this->get_field( name);
		return set;
	}

	unsigned int size() const {
		return field_list.size();
	}

	/**
	 * Returns input type for a field descriptor, that can contain any of the fields in the set.
	 * Typical usage is from derived class, where we add fields in the constructor and make auxiliary temporary instance
	 * to get the record odf the field descriptor. Simplest example:
	 *
	 * @code
	 * class EqData : public FieldSet {
	 * public:
	 * 		// fields
	 * 		Field<..> field_a;
	 * 		Field<..> field_b
	 * 		EqData() {
	 * 			add(field_a);
	 * 			add(field_b);
	 * 		}
	 * }
	 *
	 * Input::Type::Record SomEquation::input_type=
	 * 		Record("SomeEquation","equation's description")
	 * 		.declare_key("data",Input::Type::Array(EqData().make_field_descriptor_type()),"List of field descriptors.");
	 * @endcode
	 */
    Input::Type::Record make_field_descriptor_type(const std::string &equation_name) const {
    	Input::Type::Record rec = FieldCommonBase::field_descriptor_record(equation_name);
    	for(auto field : field_list) {
    		rec.declare_key(field->name(), field->get_input_type(), field->desc() );
    	}
    	return rec;
    }


    /**
     * Use @p FieldCommonBase::copy_from() to set field of the field set given by the first parameter @p dest_field_name.
     * The source field is given as the second parameter @p source. The field copies share same input descriptor list
     * and same instances of FieldBase classes but each copy can be set to different time and different limit side.
     */
    void set_field(const std::string &dest_field_name, FieldCommonBase &source) {
    	get_field(dest_field_name).copy_from(source);
    }

    /**
     * Returns pointer to the field given by name @p field_name. Throws if the field with given name is not found.
     */
    FieldCommonBase &get_field(const std::string &field_name) const {
		for(auto field : field_list)
			if (field->name() ==field_name) return *field;
		THROW(ExcUnknownField() << FieldCommonBase::EI_Field(field_name));
		return *field_list[0]; // formal to prevent compiler warning
    }


    /**
     * Collective interface to @p FieldCommonBase::set_mesh().
     */
    void set_mesh(const Mesh &mesh) {
    	for(auto field : field_list) field->set_mesh(mesh);
    }

    /**
     * Collective interface to @p FieldCommonBase::set_mesh().
     */
    void set_input_list(Input::Array input_list) {
    	for(auto field : field_list) field->set_input_list(input_list);
    }

    /**
     * Collective interface to @p FieldCommonBase::set_mesh().
     */
    void set_limit_side(LimitSide side) {
    	for(auto field : field_list) field->set_limit_side(side);
    }
    /**
     * Collective interface to @p FieldCommonBase::set_mesh().
     */
    bool changed() const {
    	bool changed_all=false;
    	for(auto field : field_list) changed_all = changed_all || field->changed();
    	return changed_all;
    }

    /**
     * Collective interface to @p FieldCommonBase::set_mesh().
     */
    bool is_constant(Region reg) const {
    	bool const_all=false;
    	for(auto field : field_list) const_all = const_all || field->is_constant(reg);
    	return const_all;
    }

    /**
     * Collective interface to @p FieldCommonBase::set_mesh().
     */
    void set_time(const TimeGovernor &time) {
    	for(auto field : field_list) field->set_time(time);
    }


private:


    /// List of all fields.
    std::vector<FieldCommonBase *> field_list;
};


/**
 * (OBSOLETE)
 * Macro to simplify call of EqDataBase::add_field method. Two forms are supported:
 *
 *
 *
 * ADD_FIELD(some_field, description);
 * ADD_FIELD(some_field, description, Default);
 *
 * The first form adds name "some_field" to the field member some_field, also adds description of the field. No default
 * value is specified, so the user must initialize the field on all regions (This is checked at the end of the method
 * EqDataBase::init_from_input.
 *
 * The second form adds also default value to the field, that is Default(".."), or Default::read_time(), other default value specifications are
 * meaningless. The automatic conversion to FieldConst is used, e.g.  Default::("0.0") is automatically converted to
 * { TYPE="FieldConst", value=[ 0.0 ] } for a vector valued field, so you get zero vector on output on regions with default value.
 */
#define ADD_FIELD(field_name, desc)                   *this += field_name.name(#field_name).desc(desc))
#define ADD_FIELD(field_name, desc, dflt)             *this += field_name.name(#field_name).desc(desc)).init_default(dflt)

#endif /* FIELD_SET_HH_ */
