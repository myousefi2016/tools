/*=========================================================================
Copyright (c) Constantine Butakoff
All rights reserved.
This software is distributed WITHOUT ANY WARRANTY; without even
the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE.  See the above copyright notice for more information.
=========================================================================*/
// Given a mesh (unstructured grid) and 3 images with x,y,z fibers, 
// create fibers at mesh vertices by sampling the images
// images are loaded sequentially, to reduce memory usage

#include <vtkSmartPointer.h>
#include <vtkDataSetReader.h>
#include <vtkXMLMultiBlockDataReader.h>
#include <vtkDataSetWriter.h>
#include <vtkProbeFilter.h>
#include <vtkCell.h>
#include <vtkPoints.h>
#include <vtkGradientFilter.h>
#include <vtkPointData.h>
#include <vtkProbeFilter.h>
#include <vtkPolyData.h>
#include <vtkFloatArray.h>
#include <vtkCompositeDataSet.h>
#include <vtkUnstructuredGrid.h>

#include <VTKCommonTools.h>
#include <vtkCallbackCommand.h>

#include <iostream>

#include <Eigen/Dense>
#include <vtkMultiBlockDataSet.h>

//WOrks on point data
//Adds float array "IsoDerivative"
void EstimateConductionVelocity(vtkDataSet* mesh, const char* isoch_array);
double GetDifferentiationStep(vtkDataSet* mesh);

//by default use order 5 (no reason)
//http://web.media.mit.edu/~crtaylor/calculator.html
void CreateSamplingPositions( Eigen::Vector3d& center, Eigen::Vector3d& direction, double h, vtkPoints* pts );
double CalculateDerivative( vtkDataArray* samples, double h );

int main(int argc, char** argv)
{
    if(argc<3)
    {
        std::cout<<"Usage: EstimateConductionVelocity mesh.vtk isochrone_array_name outmesh.vtk"<<std::endl;
        exit(-1);
    }

    const char * arrayname = "Fibers";
    
    int c=1;
    const char* meshfilename = argv[c++];
    const char* isochrone_array_name = argv[c++];
    const char* outfilename = argv[c++];
    
    std::cout<<"Mesh: "<<meshfilename<<std::endl;
    std::cout<<"Output: "<<outfilename<<std::endl;
    std::cout<<"Isochrone array: "<<isochrone_array_name<<std::endl;
    
    vtkDataSet* mesh;
        
    vtkSmartPointer<vtkXMLMultiBlockDataReader> mesh_reader_xml = vtkSmartPointer<vtkXMLMultiBlockDataReader>::New();
    if( mesh_reader_xml->CanReadFile(meshfilename) )
    {
        CommonTools::AssociateProgressFunction(mesh_reader_xml);
        std::cout<<"Reading mesh: "<<meshfilename<<std::endl;
        mesh_reader_xml->SetFileName(meshfilename);
        mesh_reader_xml->Update();

        
        vtkMultiBlockDataSet* block = vtkMultiBlockDataSet::SafeDownCast(mesh_reader_xml->GetOutput());
	std::cout<<"Hello"<<std::endl;
        std::cout<<block->GetClassName()<<std::endl;
        std::cout<<vtkUnstructuredGrid::SafeDownCast( block->GetBlock(0) )->GetNumberOfPoints()<<std::endl;
        
    
        mesh = vtkDataSet::SafeDownCast( block->GetBlock(0) );
        
    }
    else
    {
        vtkSmartPointer<vtkDataSetReader> mesh_reader = vtkSmartPointer<vtkDataSetReader>::New();
        CommonTools::AssociateProgressFunction(mesh_reader);
        std::cout<<"Reading mesh: "<<meshfilename<<std::endl;
        mesh_reader->SetFileName(meshfilename);
        mesh_reader->Update();        
        mesh  = mesh_reader->GetOutput();
    }
    

	    
    EstimateConductionVelocity(mesh, isochrone_array_name);
    
    
    vtkSmartPointer<vtkDataSetWriter> wr = vtkSmartPointer<vtkDataSetWriter>::New();
    CommonTools::AssociateProgressFunction(wr);
    std::cout<<"Output filename: "<<outfilename<<std::endl;
    wr->SetFileName(outfilename);
    wr->SetFileTypeToBinary();
    wr->SetInputData(mesh);
    wr->Write();
}



void EstimateConductionVelocity(vtkDataSet* mesh, const char* isoch_array)
{
    std::cout<<"Estimating differentiation step"<<std::endl;
    const double h = GetDifferentiationStep(mesh);
    
    //calculate the gradient of the isochrones
    vtkSmartPointer<vtkGradientFilter> grad_filt = vtkSmartPointer<vtkGradientFilter>::New();
    CommonTools::AssociateProgressFunction(grad_filt);
    grad_filt->SetInputData(mesh);
    grad_filt->SetInputArrayToProcess(0, 0, 0, vtkDataObject::FIELD_ASSOCIATION_POINTS, isoch_array);
    grad_filt->ComputeDivergenceOff();
    grad_filt->ComputeGradientOn();
    grad_filt->ComputeQCriterionOff();
    grad_filt->ComputeVorticityOff();
    grad_filt->Update();
    
    vtkDataArray *grad = grad_filt->GetOutput()->GetPointData()->GetArray("Gradients");
    
    
    vtkSmartPointer<vtkProbeFilter> prober = vtkSmartPointer<vtkProbeFilter>::New();
    prober->SetSourceData(mesh);
    prober->SetInputArrayToProcess(0,0,0,vtkDataObject::FIELD_ASSOCIATION_POINTS, isoch_array);
    
    
    vtkSmartPointer<vtkFloatArray> derivs = vtkSmartPointer<vtkFloatArray>::New();
    derivs->SetName("IsoDerivative");
    derivs->SetNumberOfComponents(1);
    derivs->SetNumberOfTuples(mesh->GetNumberOfPoints());
    
    //for every mesh vertex 
    for(vtkIdType ptid = 0; ptid<mesh->GetNumberOfPoints(); ptid++ )
    {
        if( ptid % 1000 == 0 )
        {
            std::cout<<"Processing point "<<ptid<<"/"<<mesh->GetNumberOfPoints()<<"\r";
        }
        
        Eigen::Vector3d center;
        mesh->GetPoint(ptid, center.data());
        
        Eigen::Vector3d direction;
        grad->GetTuple(ptid, direction.data());
        
        vtkSmartPointer<vtkPoints> samples;
        CreateSamplingPositions(center, direction, h, samples);

        vtkSmartPointer<vtkPolyData> pd = vtkSmartPointer<vtkPolyData>::New();
        pd->SetPoints(samples);
        prober->SetInputData(pd);
        prober->Update();
        
        const double d = CalculateDerivative(prober->GetOutput()->GetPointData()->GetScalars(), h);
        derivs->SetTuple1(ptid, d);
    }
    std::cout<<std::endl;
    
    mesh->GetPointData()->AddArray(derivs);
}




double GetDifferentiationStep(vtkDataSet* mesh)
{
    //For now I assume that all the mesh edges have equal length
    Eigen::Vector3d pt0, pt1;

    mesh->GetPoint( mesh->GetCell(0)->GetPointIds()->GetId(0), pt0.data() );
    mesh->GetPoint( mesh->GetCell(0)->GetPointIds()->GetId(1), pt1.data() );

    return (pt0-pt1).norm();
}




void CreateSamplingPositions( Eigen::Vector3d& center, Eigen::Vector3d& direction, double h, vtkPoints* pts )
{
    double displacement[] = {-2,-1,1,2};
    pts->SetNumberOfPoints(5);
    for(int i=0; i<4; i++)
    {
        Eigen::Vector3d pt = center + direction*h*displacement[i];
        pts->SetPoint(i, pt.data());
    }
}




double CalculateDerivative( vtkDataArray* samples, double h )
{
    double result = 0;
    double coef[] = {1,-8,8,-1}; //removed the samples with coefficient 0
    
    for(int i=0; i<4; i++)
    {
        result += samples->GetTuple1(i) *  coef[i];
    }
    result /= 12*h;
    
    return result;
}