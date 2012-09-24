/*!
 *
 * Copyright (C) 2007 Technical University of Liberec.  All rights reserved.
 *
 * Please make a following refer to Flow123d on your project site if you use the program for any purpose,
 * especially for academic research:
 * Flow123d, Research Centre: Advanced Remedial Technologies, Technical University of Liberec, Czech Republic
 *
 * This program is free software; you can redistribute it and/or modify it under the terms
 * of the GNU General Public License version 3 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program; if not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 021110-1307, USA.
 *
 *
 * $Id: function_interpolated_p0.cc 1567 2012-02-28 13:24:58Z jan.brezina $
 * $Revision: 1567 $
 * $LastChangedBy: jan.brezina $
 * $LastChangedDate: 2012-02-28 14:24:58 +0100 (Tue, 28 Feb 2012) $
 *
 *
 */

#include "function_interpolated_p0.hh"
#include "system/system.hh"
#include "mesh/msh_gmshreader.h"
#include "new_mesh/bih_tree.hh"
#include "new_mesh/ngh/include/intersection.h"
#include "new_mesh/ngh/include/point.h"

FunctionInterpolatedP0::FunctionInterpolatedP0() {

}

void FunctionInterpolatedP0::set_element(ElementFullIter &element){
	element_ = element;
}

void FunctionInterpolatedP0::set_source_of_interpolation(const std::string & mesh_file,
		const std::string & raw_output, const std::string & ngh_file, const std::string & bcd_file) {

	// read mesh, create tree
	MeshReader* meshReader = new GmshMeshReader();
	mesh_ = new Mesh(ngh_file, bcd_file);
	meshReader->read(mesh_file, mesh_);
	bihTree_ = new BIHTree(mesh_);

	// read pressures
	FILE* raw_output_file = xfopen(raw_output.c_str(), "rt");
	read_pressures(raw_output_file);
	xfclose(raw_output_file);

	calculate_interpolation();
}

void FunctionInterpolatedP0::read_pressures(FILE* raw_output) {
	xprintf(Msg, " - FunctionInterpolatedP0->read_pressures(FILE* raw_output)\n");

	int numElements;
	char line[ LINE_SIZE ];

	skip_to(raw_output, "$FlowField");
	xfgets(line, LINE_SIZE - 2, raw_output); //time
	xfgets(line, LINE_SIZE - 2, raw_output); //count of elements
	numElements = atoi(xstrtok(line));
	pressures_.reserve(numElements);
	for (int  i = 0; i < numElements; ++i) pressures_.push_back(0.0);
	xprintf(Msg, " - Reading pressures...");

	for (int i = 0; i < numElements; ++i) {
		int id;
		double pressure;

		xfgets(line, LINE_SIZE - 2, raw_output);

		//get element ID, presure
		id = atoi(xstrtok(line));
		pressure = atof(xstrtok(NULL));
		ElementFullIter ele = mesh_->element.find_id(id);
		pressures_[ ele.index() ] = pressure;
	}

	xprintf(Msg, " %d values of pressure read. O.K.\n", pressures_.size());
}

void FunctionInterpolatedP0::calculate_interpolation() {
	TPoint pointA(0.01, 0.01, 0.00);
	TPoint pointB(0.16, 0.16, 0.00);
	TPoint pointC(0.02, 0.02, 0.05);
	TTriangle triangle(pointA, pointB, pointC);
	std::vector<int> searchedElements;

	double pressure = calculate_element(triangle, searchedElements);
	printf("Pressure = %f\n", pressure);
}

double FunctionInterpolatedP0::calculate_element(TTriangle &element, std::vector<int> &searchedElements) {
	double elArea, iArea, pressure;
	BoundingBox elementBoundingBox = element.get_bounding_box();
	std::vector<int>::iterator it;
	TIntersectionType iType;
	TTetrahedron *tetrahedron = new TTetrahedron();

	searchedElements.erase(searchedElements.begin(), searchedElements.end());
	((BIHTree *)bihTree_)->find_elements(elementBoundingBox, searchedElements);
	std:sort(searchedElements.begin(), searchedElements.end());

	it = searchedElements.begin();
	while (it != (searchedElements.end() - 1)) {
		int idIter = *it;
		int idNext = *(it + 1);
		if (idIter == idNext) {
			searchedElements.erase(it);
		} else {
			++it;
		}
	}

	elArea = element.GetArea();
	pressure = 0.0;

	for (it = searchedElements.begin(); it!=searchedElements.end(); it++)
	{
		int id = *it;
		ElementFullIter ele = mesh_->element.full_iter( mesh_->element.begin() + id );
		if (ele->dim() == 3) {
			createTetrahedron(ele, *tetrahedron);
			GetIntersection(element, *tetrahedron, iType, iArea);
			if (iType == area) {
				pressure += pressures_[ id ] * (iArea / elArea);
			}
		}
	}

	return pressure;
}

void FunctionInterpolatedP0::createTetrahedron(ElementFullIter ele, TTetrahedron &te) {
	ASSERT(( ele->dim() == 3 ), "Dimension of element must be 3!\n");

	te.SetPoints(new TPoint(ele->node[0]->point()(0), ele->node[0]->point()(1), ele->node[0]->point()(2)),
				new TPoint(ele->node[1]->point()(0), ele->node[1]->point()(1), ele->node[1]->point()(2)),
				new TPoint(ele->node[2]->point()(0), ele->node[2]->point()(1), ele->node[2]->point()(2)),
				new TPoint(ele->node[3]->point()(0), ele->node[3]->point()(1), ele->node[3]->point()(2)) );
}


double FunctionInterpolatedP0::value(const Point &p, const unsigned int component) const
{
	return 0.0;
}

void FunctionInterpolatedP0::vector_value(const Point &p, std::vector<double> &value) const
{

}

void FunctionInterpolatedP0::value_list(const std::vector<Point>  &point_list,
					  std::vector<double>         &value_list,
					  const unsigned int  component) const
{


}

void FunctionInterpolatedP0::vector_value_list (const std::vector<Point> &point_list,
                            std::vector< std::vector<double> > &value_list) const
{

}
