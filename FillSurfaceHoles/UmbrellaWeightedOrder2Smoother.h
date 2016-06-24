/*=========================================================================
Copyright (c) Constantine Butakoff
All rights reserved.
This software is distributed WITHOUT ANY WARRANTY; without even
the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE.  See the above copyright notice for more information.
=========================================================================*/
//patch smoothing by solving 
//U^2 (v) = 0
//where U is the second order weighted umbrella operator with cotangent weights
//v - each of the patch vertices
//The patch must be a topological disk embedded in 3D
//For details see paper of Liepa, Filling Holes in Meshes
//
//Solution using iterative process from :
//Interactive Multi-Resolution Modeling on Arbitrary Meshes, Leif Kobbelt

#ifndef __UmbrellaWeightedOrder2Smoother_h
#define __UmbrellaWeightedOrder2Smoother_h

#include <vtkPolyData.h>
#include <vtkSmartPointer.h>

#include "HoleFillerDefines.h"



//uncomment to use cotangent weights during smoothing instead of scale-dependent


//updates the input mesh
class UmbrellaWeightedOrder2Smoother
{
public:
    typedef std::vector<vtkIdType> VertexIDArrayType;
    
    typedef enum {vwCotangent, vwInvEdgeLength} VertexWeightType;
    
    void SetInputMesh(vtkPolyData *mesh) {m_originalMesh = mesh;};
    void SetCoverVertexIds(VertexIDArrayType* ids) {m_coverVertexIDs = ids;};
    
    void SetTolerance( double eps ) {m_tolerance = eps; };
    double GetTolerance() {return m_tolerance; };
    
    void SetMaxIter( int i ) {m_maxIter = i; };
    int GetMaxIter() {return m_maxIter; };
    
    void Update();
    
    
    UmbrellaWeightedOrder2Smoother():m_originalMesh(NULL), m_coverVertexIDs(NULL), m_maxIter(50), m_tolerance(1e-6),
        m_weightingType(vwInvEdgeLength) {};
    
    
    bool IntersectVectors( const VertexIDArrayType& a, const VertexIDArrayType& b, VertexIDArrayType& c ) const;
        
protected:
    typedef enum {vcInterior, vcBoundary, vcExterior} VertexClassType;
 
    typedef struct __connectivity
    {
        vtkIdType originalID;
        VertexIDArrayType connectedVertices;
        VertexClassType vertexClass;
    } VertexConnectivityType;    
    
    
    typedef Eigen::Matrix<double, 3, Eigen::Dynamic> MatrixUType;
    typedef Eigen::SparseMatrix<double> SparseMatrixDoubleType;
    
    //calculate the weight for the edge v1v2
    double CalculateEdgeWeight(const Eigen::VectorXd& v_third, const Eigen::VectorXd& v1_edge, const Eigen::VectorXd& v2_edge) const;
    double CalculateEdgeWeight(vtkIdType v_third, vtkIdType v1_edge, vtkIdType v2_edge) const;
    double EdgeWeightCotangent(const Eigen::VectorXd& v_third, const Eigen::VectorXd& v1_edge, const Eigen::VectorXd& v2_edge) const;
    double EdgeWeightInvEdgeLength(const Eigen::VectorXd& v_third, const Eigen::VectorXd& v1_edge, const Eigen::VectorXd& v2_edge) const;


    void GetVertexNeighbors(vtkIdType vertexId, VertexIDArrayType& neighbors) const;
    
    void CalculateConnectivity(); //updates m_C
    void ClassifyVertex(VertexConnectivityType& v);
    
    //Fills U and weights
//    void CalculateU( MatrixUType& U, Eigen::SparseMatrix<double>& weights ); 
    //uses U and weights and fills U2
//    void CalculateU2( MatrixUType& U2, const MatrixUType& U, const Eigen::SparseMatrix<double>& weights );
    
//    double UpdateMeshPoints( const MatrixUType& U2 );
    
    vtkIdType FindVertexConnectivityLocalID( vtkIdType id ) const; //uses ids within mesh, compares to originalID
    
    void CalculateEdgeWeightMatrix( SparseMatrixDoubleType& W ) const;
    
    
private:
    typedef std::vector<VertexConnectivityType> VertexConnectivityArrayType; 
    
    vtkPolyData *m_originalMesh;
    VertexIDArrayType* m_coverVertexIDs; //IDs within the original mesh
    
    double m_tolerance;
    int m_maxIter;

    VertexWeightType m_weightingType;
    
    VertexConnectivityArrayType m_C; //stores IDs within the original mesh
};


#endif
