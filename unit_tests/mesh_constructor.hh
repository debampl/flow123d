/*
 * mesh_constructor.hh
 *
 *  Created on: Jul 4, 2016
 *      Author: jb
 */

#ifndef UNIT_TESTS_MESH_CONSTRUCTOR_HH_
#define UNIT_TESTS_MESH_CONSTRUCTOR_HH_

#include <iostream>

#include "input/reader_to_storage.hh"
#include "input/type_record.hh"
#include "io/msh_basereader.hh"
#include "mesh/mesh.h"


namespace IT = Input::Type;


/// Helper method, construct Input::Record accessor from input string.
Input::Record get_record_accessor(const std::string &input_str, Input::FileFormat format) {
	istringstream is(input_str);
    Input::ReaderToStorage reader;
    IT::Record &in_rec = const_cast<IT::Record &>(Mesh::get_input_type());
    in_rec.finish();
    reader.read_stream(is, in_rec, format);

    return reader.get_root_interface<Input::Record>();
}

/// Construct reader.
std::shared_ptr<BaseMeshReader> reader_constructor(const std::string &input_str,
		Input::FileFormat format = Input::FileFormat::format_JSON) {
    return BaseMeshReader::reader_factory( get_record_accessor(input_str, format).val<FilePath>("mesh_file" ));
}


/// Construct mesh with data. Use mesh_factory that fill data and set topology of mesh.
Mesh * mesh_full_constructor(const std::string &input_str,
		Input::FileFormat format = Input::FileFormat::format_JSON) {
    return BaseMeshReader::mesh_factory( get_record_accessor(input_str, format) );
}


/// Construct mesh without filling data, only set mesh input record.
Mesh * mesh_constructor(const std::string &input_str,
		Input::FileFormat format = Input::FileFormat::format_JSON) {
	Mesh * mesh = new Mesh( get_record_accessor(input_str, format) );
    return mesh;
}


#endif /* UNIT_TESTS_MESH_CONSTRUCTOR_HH_ */
