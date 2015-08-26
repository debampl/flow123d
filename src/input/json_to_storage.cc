/*
 * json_to_storage.cc
 *
 *  Created on: May 7, 2012
 *      Author: jb
 */

#include <cstdint>
#include <limits>
#include <boost/iostreams/device/file.hpp>
#include <boost/iostreams/filtering_stream.hpp>

#include "json_to_storage.hh"
#include "input/comment_filter.hh"

#include "json_spirit/json_spirit_error_position.h"

namespace Input {
using namespace std;
using namespace internal;



/********************************************
 * Implementation of PathBase
 */
PathBase::PathBase() {
	path_.push_back( make_pair( (int)(-1), string("/") ) );
}


void PathBase::output(ostream &stream) const {
    if (level() == 0) {
        stream << "/";
        return;
    }

    for(vector< pair<int, string> >::const_iterator it = path_.begin()+1; it != path_.end(); ++it) {
        if ( it->first < 0 ) {
            stream << "/" << it->second;
        } else {
            stream << "/" << it->first;
        }
    }
}



string PathBase::as_string() const {
    stringstream ss;
    output(ss);
    return ss.str();
}



void PathBase::go_to_root() {
	while (path_.size() > 1) {
		this->up();
	}
}



/********************************************
 * Implementation of  internal::PathJSON
 */


PathJSON::PathJSON(istream &in)
: PathJSON()
{
    io::filtering_istream filter_in;

    filter_in.push(uncommenting_filter());
    filter_in.push(in);

    Node root_node;

    try {
        json_spirit::read_or_throw( filter_in, root_node);
    } catch (json_spirit::Error_position &e ) {
        THROW( JSONToStorage::ExcNotJSONFormat() << JSONToStorage::EI_JSONLine(e.line_) << JSONToStorage::EI_JSONColumn(e.column_)
        	<< JSONToStorage::EI_JSONReason(e.reason_));
    }

    nodes_.push_back( new Node(root_node) );
}


PathJSON::PathJSON()
: PathBase()
{
    /* from json_spirit_value.hh:
     * enum Value_type{ obj_type, array_type, str_type, bool_type, int_type, real_type, null_type };
     */
    json_type_names.push_back("JSON object");
    json_type_names.push_back("JSON array");
    json_type_names.push_back("JSON string");
    json_type_names.push_back("JSON bool");
    json_type_names.push_back("JSON int");
    json_type_names.push_back("JSON real");
    json_type_names.push_back("JSON null");
}


bool PathJSON::down(unsigned int index)
{
    const Node * head_node = nodes_.back();
    const json_spirit::mArray &array = head_node->get_array(); // the type should be checked in make_storage

    if (  index >= array.size()) return false;
    path_.push_back( make_pair( index, string("") ) );
    nodes_.push_back( &( array[index] ) );
    return true;
}


bool PathJSON::down(const string& key)
{
    const Node * head_node = nodes_.back();
    const json_spirit::mObject &obj = head_node->get_obj(); // the type should be checked in make_storage

    json_spirit::mObject::const_iterator it = obj.find(key);
    if (it == obj.end()) {
        return false;
    } else {
        path_.push_back( make_pair( (int)(-1), key) );
        nodes_.push_back( &( it->second ) );
    }
    return true;
}


void PathJSON::up()
{
    if (path_.size() > 1) {
        path_.pop_back();
        nodes_.pop_back();
    }
}


/**
 * This returns path to reference given by address in ref_address.
 *
 */
PathBase * PathJSON::find_ref_node()
{
    namespace ba = boost::algorithm;

    const Node * head_node = nodes_.back();
    if (head_node->type() != json_spirit::obj_type) return NULL;
    const json_spirit::mObject &obj = head_node->get_obj();
    if (obj.size() != 1) return NULL;
    if (obj.begin()->first != "REF") return NULL;

    const Node &ref_node = obj.begin()->second;
    if (ref_node.type() != json_spirit::str_type) {
        THROW( ExcRefOfWrongType() << EI_ErrorAddress(this->as_string()) );

    }
    string ref_address = ref_node.get_str();

    PathJSON * ref_path = this->clone();
    string::size_type pos = 0;
    string::size_type new_pos = 0;
    string address = ref_address + '/';
    string tmp_str;
    bool relative_ref = false;

    std::set<string>::iterator it = previous_references_.find(ref_address);
    if (it == previous_references_.end()) {
    	ref_path->previous_references_.insert(ref_address);
    } else {
    	THROW( ExcReferenceNotFound() << EI_RefAddress(this->as_string()) << EI_ErrorAddress(ref_path->as_string()) << EI_RefStr(ref_address)
    	       << EI_Specification("cannot follow reference") );
    }

    while ( ( new_pos=address.find('/',pos) ) != string::npos ) {
        tmp_str = address.substr(pos, new_pos - pos);
        if (pos==0 && tmp_str == "") {
            // absolute path
            ref_path->go_to_root();

        } else if ( ba::all( tmp_str, ba::is_digit()) ) {
            // integer == index in array
            if ( !ref_path->is_array_type() ) {
                THROW( ExcReferenceNotFound() << EI_RefAddress(this->as_string()) << EI_ErrorAddress(ref_path->as_string()) << EI_RefStr(ref_address)
                        << EI_Specification("there should be Array") );
            }

            if ( !ref_path->down( atoi(tmp_str.c_str()) ) ) {
                THROW( ExcReferenceNotFound() << EI_RefAddress(this->as_string()) << EI_ErrorAddress(ref_path->as_string()) << EI_RefStr(ref_address)
                        << EI_Specification("index out of size of Array") );
            }

        } else if (tmp_str == "..") {
        	relative_ref = true;
            if (ref_path->level() <= 0 ) {
                THROW( ExcReferenceNotFound() << EI_RefAddress(this->as_string()) << EI_ErrorAddress(ref_path->as_string()) << EI_RefStr(ref_address)
                        << EI_Specification("can not go up from root") );
            }
            ref_path->up();

        } else {
            if ( !ref_path->is_record_type() )
                THROW( ExcReferenceNotFound() << EI_RefAddress(this->as_string()) << EI_ErrorAddress(ref_path->as_string()) << EI_RefStr(ref_address)
                        << EI_Specification("there should be Record") );
            if ( !ref_path->down(tmp_str) )
                THROW( ExcReferenceNotFound() << EI_RefAddress(this->as_string()) << EI_ErrorAddress(ref_path->as_string()) << EI_RefStr(ref_address)
                        << EI_Specification("key '"+tmp_str+"' not found") );
        }
        pos = new_pos+1;
    }
    if (relative_ref) {
    	xprintf(Msg, "Referencing '%s' to '%s'.\n", this->as_string().c_str(), ref_path->as_string().c_str());
    }
    return ref_path;
}



bool PathJSON::is_null_type() const {
	return head()->type() == json_spirit::null_type;
}


bool PathJSON::get_bool_value() const {
    if (head()->type() == json_spirit::bool_type) {
        return head()->get_bool();
    } else {
        THROW( JSONToStorage::ExcInputError() << JSONToStorage::EI_Specification("The value should be 'JSON bool', but we found: ")
                << JSONToStorage::EI_ErrorAddress(this->as_string()) << JSONToStorage::EI_JSON_Type( get_node_type() )
             );
    }
	return false;
}



std::int64_t PathJSON::get_int_value() const {
    if (head()->type() == json_spirit::int_type) {
        return head()->get_int64();
    } else {
        THROW( JSONToStorage::ExcInputError() << JSONToStorage::EI_Specification("The value should be 'JSON int', but we found: ")
                << JSONToStorage::EI_ErrorAddress(this->as_string()) << JSONToStorage::EI_JSON_Type( get_node_type() )
             );
    }
	return 0;
}



double PathJSON::get_double_value() const {
    auto value_type = head()->type();
    if (    value_type== json_spirit::real_type
         || value_type == json_spirit::int_type) {
        return head()->get_real();
    } else {
        THROW( JSONToStorage::ExcInputError() << JSONToStorage::EI_Specification("The value should be 'JSON real', but we found: ")
                << JSONToStorage::EI_ErrorAddress(this->as_string()) << JSONToStorage::EI_JSON_Type( get_node_type() )
        	 );
    }
	return 0.0;
}



std::string PathJSON::get_string_value() const {
    if (head()->type() == json_spirit::str_type) {
        return head()->get_str();
    } else {
        THROW( JSONToStorage::ExcInputError() << JSONToStorage::EI_Specification("The value should be 'JSON string', but we found: ")
                << JSONToStorage::EI_ErrorAddress(this->as_string()) << JSONToStorage::EI_JSON_Type( get_node_type() )
             );
    }
	return "";
}



std::string PathJSON::get_node_type() const {
	return json_type_names[ head()->type() ];
}



bool PathJSON::get_record_key_set(std::set<std::string> &keys_list) const {
	if ( this->is_record_type() ) {
    	const json_spirit::mObject & j_map = head()->get_obj();
        json_spirit::mObject::const_iterator map_it;
        std::set<string>::iterator set_it;

        for( map_it = j_map.begin(); map_it != j_map.end(); ++map_it) {
        	keys_list.insert(map_it->first);
        }

        return true;
    }

	return false;
}



int PathJSON::get_array_size() const {
    if (head()->type() == json_spirit::array_type) {
        const json_spirit::mArray & j_array = head()->get_array();
        return j_array.size();
    }

    return -1;
}



bool PathJSON::is_record_type() const {
	return head()->type() == json_spirit::obj_type;
}



bool PathJSON::is_array_type() const {
	return head()->type() == json_spirit::array_type;
}



PathJSON * PathJSON::clone() const {
	return new PathJSON(*this);
}



std::string PathJSON::get_descendant_name() const {
	std::string desc_name = "";
	PathBase *type_path = this->clone();
	if ( type_path->down("TYPE") ) {
		desc_name = type_path->get_string_value();
	}
	delete type_path;

	return desc_name;
}



void PathJSON::remember_reference() {
	previous_references_.insert(as_string());
}



std::ostream& operator<<(std::ostream& stream, const PathJSON& path) {
    path.output(stream);
    return stream;
}


/********************************************
 * Implementation of PathYAML
 */

PathYAML::PathYAML(istream &in)
: PathBase()
{
	PathYAML::Node root_node = YAML::Load( in );
    nodes_.push_back( new Node(root_node) );
}


bool PathYAML::down(unsigned int index) {
    const Node & head_node = *( nodes_.back() );
    ASSERT(head_node.IsSequence(), "Head node must be of type Array.\n");

    if ( index >= head_node.size() ) return false;
    path_.push_back( make_pair( index, string("") ) );
    nodes_.push_back( new Node( head_node[index] ) );

    return true;
}


bool PathYAML::down(const string& key) {
    const Node & head_node = *( nodes_.back() );
    ASSERT(head_node.IsMap(), "Head node must be of type Record.\n");

    if (head_node[key]) {
    	path_.push_back( make_pair( (int)(-1), key) );
    	nodes_.push_back( new Node( head_node[key] ) );
    } else {
        return false;
    }
    return true;
}


void PathYAML::up() {
    if (path_.size() > 1) {
        path_.pop_back();
        int size = nodes_.size();
        const Node * node = nodes_.back();
        delete node;
        nodes_.resize(size-1);
    }
}


bool PathYAML::is_null_type() const {
	return head()->IsNull();
}


bool PathYAML::get_bool_value() const {
	if (head()->IsScalar()) {
		try {
			return head()->as<bool>();
		} catch (YAML::Exception) {
	        THROW( JSONToStorage::ExcInputError() << JSONToStorage::EI_Specification("The value should be 'YAML bool', but we found: ")
	                << JSONToStorage::EI_ErrorAddress(this->as_string()) << JSONToStorage::EI_JSON_Type( get_node_type() )
	             );
		}
	} else {
        THROW( JSONToStorage::ExcInputError() << JSONToStorage::EI_Specification("The value should be 'YAML bool', but we found: ")
                << JSONToStorage::EI_ErrorAddress(this->as_string()) << JSONToStorage::EI_JSON_Type( get_node_type() )
             );
	}
	return false;
}


std::int64_t PathYAML::get_int_value() const {
	if (head()->IsScalar()) {
		try {
			return head()->as<std::int64_t>();
		} catch (YAML::Exception) {
	        THROW( JSONToStorage::ExcInputError() << JSONToStorage::EI_Specification("The value should be 'YAML int', but we found: ")
	                << JSONToStorage::EI_ErrorAddress(this->as_string()) << JSONToStorage::EI_JSON_Type( get_node_type() )
	             );
		}
	} else {
        THROW( JSONToStorage::ExcInputError() << JSONToStorage::EI_Specification("The value should be 'YAML int', but we found: ")
                << JSONToStorage::EI_ErrorAddress(this->as_string()) << JSONToStorage::EI_JSON_Type( get_node_type() )
             );
	}
	return 0;
}


double PathYAML::get_double_value() const {
	if (head()->IsScalar()) {
		try {
			return head()->as<double>();
		} catch (YAML::Exception) {
	        THROW( JSONToStorage::ExcInputError() << JSONToStorage::EI_Specification("The value should be 'YAML double', but we found: ")
	                << JSONToStorage::EI_ErrorAddress(this->as_string()) << JSONToStorage::EI_JSON_Type( get_node_type() )
	             );
		}
	} else {
        THROW( JSONToStorage::ExcInputError() << JSONToStorage::EI_Specification("The value should be 'YAML double', but we found: ")
                << JSONToStorage::EI_ErrorAddress(this->as_string()) << JSONToStorage::EI_JSON_Type( get_node_type() )
             );
	}
	return 0.0;
}


std::string PathYAML::get_string_value() const {
	if (head()->IsScalar()) {
		try {
			return head()->as<std::string>();
		} catch (YAML::Exception) {
	        THROW( JSONToStorage::ExcInputError() << JSONToStorage::EI_Specification("The value should be 'YAML string', but we found: ")
	                << JSONToStorage::EI_ErrorAddress(this->as_string()) << JSONToStorage::EI_JSON_Type( get_node_type() )
	             );
		}
	} else {
        THROW( JSONToStorage::ExcInputError() << JSONToStorage::EI_Specification("The value should be 'YAML string', but we found: ")
                << JSONToStorage::EI_ErrorAddress(this->as_string()) << JSONToStorage::EI_JSON_Type( get_node_type() )
             );
	}
	return "";
}


std::string PathYAML::get_node_type() const {
	switch (head()->Type()) {
	  case YAML::NodeType::Null: return "YAML null";
	  case YAML::NodeType::Scalar: return "other scalar type";
	  case YAML::NodeType::Sequence: return "YAML sequence";
	  case YAML::NodeType::Map: return "YAML map";
	  default: return "undefined type";
	}
}


bool PathYAML::get_record_key_set(std::set<std::string> &keys_list) const {
	if ( head()->IsMap() ) {
		for (YAML::const_iterator it=head()->begin(); it!=head()->end(); ++it) {
			keys_list.insert( it->first.as<std::string>() );
		}
        return true;
    }

	return false;
}


int PathYAML::get_array_size() const {
	if (head()->IsSequence()) {
		return head()->size();
	}
	return -1;
}


bool PathYAML::is_record_type() const {
	return head()->IsMap();
}


bool PathYAML::is_array_type() const {
	return head()->IsSequence();
}


PathYAML * PathYAML::clone() const {
	return new PathYAML(*this);
}


PathBase * PathYAML::find_ref_node()
{
    return NULL;
}



std::string PathYAML::get_descendant_name() const {
	const Node & head_node = *( nodes_.back() );
	std::string tag = head_node.Tag();
	if (tag == "?") {
		return "";
	} else {
		return tag.erase(0, 1); // tag starts with '!' char
	}
}



std::ostream& operator<<(std::ostream& stream, const PathYAML& path) {
    path.output(stream);
    return stream;
}


/********************************************
 * Implementation of public part of JSONToStorage
 */

JSONToStorage::JSONToStorage()
: storage_(nullptr),
  root_type_(nullptr)
{}



JSONToStorage::JSONToStorage(const FilePath &in_file, const Type::TypeBase &root_type)
: JSONToStorage()
{
	std::string fname = in_file;
	std::string extension = fname.substr(fname.find_last_of(".") + 1);
	FileFormat format;
	if (extension == "con") {
		format = FileFormat::format_JSON;
	} else if (extension == "yaml") {
		format = FileFormat::format_YAML;
	} else {
		THROW(ExcInputMessage() << EI_Message("Invalid extension of file " + fname + ".\nMust be 'con' or 'yaml'."));
	}

	std::ifstream in(fname.c_str());
    if (! in) {
    	THROW(ExcInputMessage() << EI_Message("Can not open main input file: '" + fname + "'.\n"));
    }
	read_stream(in, root_type, format);
}



JSONToStorage::JSONToStorage( const string &str, const Type::TypeBase &root_type, FileFormat format)
: JSONToStorage()
{
	try {
		istringstream is(str);
		read_stream(is, root_type, format);
	} catch (ExcNotJSONFormat &e) {
		e << EI_File("STRING: "+str); throw;
	}
}



void JSONToStorage::read_stream(istream &in, const Type::TypeBase &root_type, FileFormat format)
{
    namespace io = boost::iostreams;
    ASSERT(storage_==nullptr," ");

    // finish all lazy input types
    Input::Type::TypeBase::lazy_finish();

    PathBase * root_path;
	if (format == FileFormat::format_JSON) {
		root_path = new PathJSON(in);
	} else {
		root_path = new PathYAML(in);
	}

	std::unique_ptr<PathBase> root_path_ptr(root_path);

    root_type_ = &root_type;
	try {
	    storage_ = make_storage(*root_path_ptr, root_type_);
	} catch (ExcInputError &e) {
		if (format == FileFormat::format_JSON) {
			e << EI_Format("JSON");
		} else {
			e << EI_Format("YAML");
		}
		throw;
	}

    ASSERT(  storage_ != nullptr, "Internal error in Input reader, the storage pointer is NULL after reading the stream.\n");
}






/********************************************
 * Implementation of private part of JSONToStorage - make_storage dispatch
 */


StorageBase * JSONToStorage::make_storage(PathBase &p, const Type::TypeBase *type)
{
    ASSERT(type != NULL, "Can not dispatch, NULL pointer to TypeBase.\n");

    // find reference node, if doesn't exist return NULL
    PathBase * ref_path = p.find_ref_node();
    if (ref_path) {
        // todo: mark passed references and check cyclic references

        // dereference and take data from there
    	StorageBase * storage = make_storage( *ref_path, type );
    	delete ref_path;
        return storage;
    }

    // return Null storage if there is null on the current location
    if (p.is_null_type())
        return new StorageNull();

    // dispatch types
    if (typeid(*type) == typeid(Type::Record)) {
        return make_storage(p, static_cast<const Type::Record *>(type) );
    } else
    if (typeid(*type) == typeid(Type::Array)) {
        return make_storage(p, static_cast<const Type::Array *>(type) );
    } else
    if (typeid(*type) == typeid(Type::Integer)) {
        return make_storage(p, static_cast<const Type::Integer *>(type) );
    } else
    if (typeid(*type) == typeid(Type::Double)) {
        return make_storage(p, static_cast<const Type::Double *>(type) );
    } else
    if (typeid(*type) == typeid(Type::Bool)) {
        return make_storage(p, static_cast<const Type::Bool *>(type) );
    } else
    if (typeid(*type) == typeid(Type::Selection)) {
        return make_storage(p, static_cast<const Type::Selection *>(type) );
    } else {
    	const Type::AbstractRecord * abstract_record_type = dynamic_cast<const Type::AbstractRecord *>(type);
    	if (abstract_record_type != NULL ) return make_storage(p, abstract_record_type );

        const Type::String * string_type = dynamic_cast<const Type::String *>(type);
        if (string_type != NULL ) return make_storage(p, string_type );

        // default -> error
        ASSERT(false, "Unknown descendant of TypeBase class, name: %s\n", typeid(type).name());
    }

    return new StorageNull();
}


StorageBase * JSONToStorage::make_storage(PathBase &p, const Type::Record *record)
{
	std::set<string> keys_to_process;
	if ( p.get_record_key_set(keys_to_process) ) {
        std::set<string>::iterator set_it;

        /*Type::Record::KeyIter key_it;
        if ( record->has_key_iterator("TYPE", key_it) && record->auto_conversion_key_iter() != record->end() ) {
            PathBase *type_path = p->clone();
            if ( type_path.down( "TYPE" ) ) {
                try {
                	if ( type_path.get_string_value() != record->type_name() ) {
                		xprintf(UsrErr, "Invalid value of TYPE key of record %s.", record->type_name().c_str());
                	}
                    make_storage(type_path, key_it->type_.get() )->get_int();
                } catch(Type::Selection::ExcSelectionKeyNotFound &e) {
                	return record_automatic_conversion(p, record);
                }
            }
            else {  // automatic conversion
            	return record_automatic_conversion(p, record);
            }
        }*/

        StorageArray *storage_array = new StorageArray(record->size());
        // check individual keys
        for( Type::Record::KeyIter it= record->begin(); it != record->end(); ++it) {
        	// remove processed key from keys_to_process
        	set_it = keys_to_process.find(it->key_);
        	if (set_it != keys_to_process.end()) {
        		keys_to_process.erase(set_it);
        	}

            if ( p.down(it->key_) ) {
                // key on input => check & use it
                storage_array->new_item(it->key_index, make_storage(p, it->type_.get()) );
                p.up();
            } else {
                // key not on input
                if (it->default_.is_obligatory() ) {
                    THROW( ExcInputError() << EI_Specification("Missing obligatory key '"+ it->key_ +"'.")
                            << EI_ErrorAddress(p.as_string()) << EI_InputType(record->desc()) );
                } else if (it->default_.has_value_at_declaration() ) {
                   storage_array->new_item(it->key_index,
                           make_storage_from_default( it->default_.value(), it->type_.get() ) );
                } else { // defalut - optional or default at read time
                    // set null
                    storage_array->new_item(it->key_index, new StorageNull() );
                }
            }
        }

        for( set_it = keys_to_process.begin(); set_it != keys_to_process.end(); ++set_it) {
        	xprintf(Warn, "Unprocessed key '%s' in record '%s'.\n", (*set_it).c_str(), p.as_string().c_str() );
        }

        return storage_array;

    } else { // automatic conversion
    	return record_automatic_conversion(p, record);
    }
    // possibly construction of reduced record
}


StorageBase * JSONToStorage::record_automatic_conversion(PathBase &p, const Type::Record *record)
{
	Type::Record::KeyIter auto_key_it = record->auto_conversion_key_iter();
	if ( auto_key_it != record->end() ) {
	    try {
			StorageArray *storage_array = new StorageArray(record->size());
			for( Type::Record::KeyIter it= record->begin(); it != record->end(); ++it) {
				if ( it == auto_key_it ) {
					// one key is initialized by input
					storage_array->new_item(it->key_index, make_storage(p, it->type_.get()) );
				} else if (it->default_.has_value_at_declaration() ) {
					// other key from default values
					storage_array->new_item(it->key_index,
							make_storage_from_default( it->default_.value(), it->type_.get() ) );
				 } else { // defalut - optional or default at read time
					 ASSERT( ! it->default_.is_obligatory() ,
							 "Obligatory key: '%s' in auto-convertible record, wrong check during finish().", it->key_.c_str());
					 // set null
					 storage_array->new_item(it->key_index, new StorageNull() );
				 }
			}

			return storage_array;
	    } catch (ExcInputError &e ) {
	        THROW( ExcAutomaticConversionError() << EI_RecordName(record->type_name()) << EI_InputErrorMessage(e.what()) );
	    }

	} else {
	    THROW( ExcInputError() << EI_Specification("The value should be '" + p.map_name() + "', but we found: ")
	            << EI_ErrorAddress(p.as_string()) << EI_JSON_Type( p.get_node_type() )
				<< EI_InputType( record->desc()) );
	}

	return NULL;
}



StorageBase * JSONToStorage::make_storage(PathBase &p, const Type::AbstractRecord *abstr_rec)
{
	if ( p.is_record_type() ) {

		string descendant_name = p.get_descendant_name();
		if ( descendant_name == "" ) {
			if ( ! abstr_rec->get_selection_default().has_value_at_declaration() ) {
				THROW( ExcInputError() << EI_Specification("Missing key 'TYPE' in AbstractRecord.") << EI_ErrorAddress(p.as_string()) << EI_InputType(abstr_rec->desc()) );
			} else { // auto conversion
				return abstract_rec_automatic_conversion(p, abstr_rec);
			}
		} else {
			try {
				unsigned int descendant_index = (unsigned int)abstr_rec->get_type_selection().name_to_int( descendant_name );
				return make_storage(p, &( abstr_rec->get_descendant(descendant_index) ) );
			} catch (ExcInputError & e) {
				e << EI_InputType(abstr_rec->get_type_selection().desc());
				throw;
			} catch (Type::Selection::ExcSelectionKeyNotFound &exc) {
				THROW( ExcInputError() << EI_Specification("Wrong value '" + descendant_name + "' of the Selection.")
						<< EI_ErrorAddress(p.as_string()) << EI_JSON_Type( "" ) << EI_InputType(abstr_rec->get_type_selection().desc()) );
			}
		}
	} else {
		if ( ! abstr_rec->get_selection_default().has_value_at_declaration() ) {
			THROW( ExcInputError() << EI_Specification("The value should be 'JSON object', but we found: ") // TODO: replace JSON object
				<< EI_ErrorAddress(p.as_string()) << EI_JSON_Type( p.get_node_type() ) << EI_InputType(abstr_rec->desc()) );
		} else { // auto conversion
			return abstract_rec_automatic_conversion(p, abstr_rec);
		}
	}

	return NULL;
}



StorageBase * JSONToStorage::abstract_rec_automatic_conversion(PathBase &p, const Type::AbstractRecord *abstr_rec)
{
    // perform automatic conversion
    const Type::Record *default_child = abstr_rec->get_default_descendant();
    if (! default_child) THROW(ExcInputError()
    		<< EI_Specification("Auto conversion of AbstractRecord not allowed.\n")
    		<< EI_ErrorAddress(p.as_string())
    		<< EI_InputType(abstr_rec->desc())
    		);
    return make_storage(p, default_child );
}



StorageBase * JSONToStorage::make_storage(PathBase &p, const Type::Array *array)
{
	int arr_size;
	if ( (arr_size = p.get_array_size()) != -1 ) {
        if ( array->match_size( arr_size ) ) {
          // copy the array and check type of values
          StorageArray *storage_array = new StorageArray(arr_size);
          for( int idx=0; idx < arr_size; idx++)  {
              p.down(idx);
              const Type::TypeBase &sub_type = array->get_sub_type();
              storage_array->new_item(idx, make_storage(p, &sub_type) );
              p.up();
          }
          return storage_array;

        } else {
            THROW( ExcInputError()
                    << EI_Specification("Do not fit into size limits of the Array.")
                    << EI_ErrorAddress(p.as_string()) << EI_InputType(array->desc()) );
        }
    } else {
        // try automatic conversion to array with one element
        if ( array->match_size( 1 ) ) {
            StorageArray *storage_array = new StorageArray(1);
            const Type::TypeBase &sub_type = array->get_sub_type();
            storage_array->new_item(0, make_storage(p, &sub_type) );

            return storage_array;
        } else {
            THROW( ExcInputError() << EI_Specification("Automatic conversion to array not allowed. The value should be '" + p.sequence_name() + "', but we found: ")
                    << EI_ErrorAddress(p.as_string()) << EI_JSON_Type( p.get_node_type() ) << EI_InputType(array->desc()) );
        }
    }

    return NULL;
}



StorageBase * JSONToStorage::make_storage(PathBase &p, const Type::Selection *selection)
{
    string item_name;
	try {
		item_name = p.get_string_value();
		int value = selection->name_to_int( item_name  );
		return new StorageInt( value );
	} catch (ExcInputError & e) {
		e << EI_InputType(selection->desc());
		throw;
	} catch (Type::Selection::ExcSelectionKeyNotFound &exc) {
		THROW( ExcInputError() << EI_Specification("Wrong value '" + item_name + "' of the Selection.")
				<< EI_ErrorAddress(p.as_string()) << EI_JSON_Type( "" ) << EI_InputType(selection->desc()) );
	}

    return NULL;
}



StorageBase * JSONToStorage::make_storage(PathBase &p, const Type::Bool *bool_type)
{
	try {
		return new StorageBool( p.get_bool_value() );
	}
	catch (ExcInputError & e) {
		e << EI_InputType(bool_type->desc());
		throw;
	}
    return NULL;
}



StorageBase * JSONToStorage::make_storage(PathBase &p, const Type::Integer *int_type)
{
	std::int64_t value;
	try {
		value = p.get_int_value();
	}
	catch (ExcInputError & e) {
		e << EI_InputType(int_type->desc());
		throw;
	}

	if ( int_type->match(value) )
	{
		return new StorageInt( value );
	} else {
		THROW( ExcInputError() << EI_Specification("Value out of bounds.") << EI_ErrorAddress(p.as_string())
				<< EI_InputType(int_type->desc()) );
	}

    return NULL;
}



StorageBase * JSONToStorage::make_storage(PathBase &p, const Type::Double *double_type)
{
    double value;

	try {
		value = p.get_double_value();
	}
	catch (ExcInputError & e) {
		e << EI_InputType(double_type->desc());
		throw;
	}

    if (double_type->match(value)) {
        return new StorageDouble( value );
    } else {
        THROW( ExcInputError() << EI_Specification("Value out of bounds.") << EI_ErrorAddress(p.as_string())
        		<< EI_InputType(double_type->desc()) );
    }

    return NULL;
}



StorageBase * JSONToStorage::make_storage(PathBase &p, const Type::String *string_type)
{
	string value;
	try {
		value = p.get_string_value();
	}
	catch (ExcInputError & e) {
		e << EI_InputType(string_type->desc());
		throw;
	}

	if (string_type->match(value))
		return new StorageString( value );
	else
		THROW( ExcInputError() << EI_Specification("Output file can not be given by absolute path: '" + value + "'")
						<< EI_ErrorAddress(p.as_string()) << EI_JSON_Type("") << EI_InputType(string_type->desc()) );

	return NULL;
}



StorageBase * JSONToStorage::make_storage_from_default(const string &dflt_str, const Type::TypeBase *type) {
    try {
    	/*
    	// Possible simplification of this method (need default strings to be valid JSON)
    	JSONToStorage  tmp_storage(dflt_str, *type);
    	return tmp_storage.storage_;
		*/

        // an auto-convertible AbstractRecord can be initialized form default value
    	const Type::AbstractRecord *a_record = dynamic_cast<const Type::AbstractRecord *>(type);
    	if (a_record != NULL ) {
    		ASSERT( a_record->get_selection_default().has_value_at_declaration(),
    				"Can not initialize (non-auto-convertible) AbstractRecord '%s' by default value\n", type->type_name().c_str() );
            return make_storage_from_default( dflt_str, a_record->get_default_descendant() );
        } else
        if (typeid(*type) == typeid(Type::Record) ) {
            // an auto-convertible Record can be initialized form default value
            const Type::Record *record = static_cast<const Type::Record *>(type);
            Type::Record::KeyIter auto_key_it = record->auto_conversion_key_iter();

            ASSERT( auto_key_it != record->end(), "Can not initialize (non-auto-convertible) Record '%s' by default value\n",
            		type->type_name().c_str());
			StorageArray *storage_array = new StorageArray(record->size());
			for( Type::Record::KeyIter it= record->begin(); it != record->end(); ++it) {
				if ( it == auto_key_it ) {
					// one key is initialized by the record default string
					storage_array->new_item(it->key_index, make_storage_from_default(dflt_str, it->type_.get()) );
				} else {

					ASSERT( ! it->default_.is_obligatory(),
							"Missing default value for key: '%s' in auto-convertible record, wrong check during finish().", it->key_.c_str());

					if (it->default_.has_value_at_declaration() ) {
					   storage_array->new_item(it->key_index,
							   make_storage_from_default( it->default_.value(), it->type_.get() ) );
					} else { // defalut - optional or default at read time
						// set null
						storage_array->new_item(it->key_index, new StorageNull() );
					}
				}
			}

			return storage_array;
        } else
        if (typeid(*type) == typeid(Type::Array) ) {
            const Type::Array *array = static_cast<const Type::Array *>(type);
            if ( array->match_size(1) ) {
               // try auto conversion to array
                StorageArray *storage_array = new StorageArray(1);
                const Type::TypeBase &sub_type = array->get_sub_type();
                storage_array->new_item(0, make_storage_from_default(dflt_str, &sub_type) );
                return storage_array;
            } else {
            	THROW(ExcInputMessage() << EI_Message("Can not initialize Array '" + type->type_name() + "' by default value, size 1 not allowed.\n"));
            }

        } else
        if (typeid(*type) == typeid(Type::Integer)) {
            return new StorageInt( static_cast<const Type::Integer *>(type) ->from_default(dflt_str) );
        } else
        if (typeid(*type) == typeid(Type::Double)) {
            return new StorageDouble( static_cast<const Type::Double *>(type) ->from_default(dflt_str) );
        } else
        if (typeid(*type) == typeid(Type::Bool)) {
            return new StorageBool( static_cast<const Type::Bool *>(type) ->from_default(dflt_str) );
        } else
        if (typeid(*type) == typeid(Type::Selection)) {
                return new StorageInt( static_cast<const Type::Selection *>(type) ->from_default(dflt_str) );
        } else {
            const Type::String * string_type = dynamic_cast<const Type::String *>(type);
            if (string_type != NULL ) return new StorageString( string_type->from_default(dflt_str) );

            // default error
            ASSERT(false, "Can not store default value for type: %s\n", typeid(type).name());
        }


    } catch (Input::Type::ExcWrongDefault & e) {
        // message to distinguish exceptions thrown during Default value check at declaration
        xprintf(Msg, "Wrong default value while reading an input stream:\n");
        e << EI_KeyName("UNKNOWN KEY");
        throw;
    }

    return NULL;
}



} // namespace Input
