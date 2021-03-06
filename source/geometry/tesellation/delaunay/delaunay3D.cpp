/*
	================================================================================
	This software is released under the LGPL-3.0 license: http://www.opensource.org/licenses/lgpl-3.0.html

	Copyright (c) 2012, Jose Esteve. http://www.joesfer.com

	This library is free software; you can redistribute it and/or
	modify it under the terms of the GNU Lesser General Public
	License as published by the Free Software Foundation; either
	version 3.0 of the License, or (at your option) any later version.

	This library is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	Lesser General Public License for more details.

	You should have received a copy of the GNU Lesser General Public
	License along with this library; if not, write to the Free Software
	Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
	================================================================================
*/

#include <math/algebra/matrix/matrix4.h>
#include <math/algebra/matrix/matrix5.h>
#include <math/algebra/vector/vector3.h>
#include <geometry/tessellation/delaunay/delaunay3D.h>
#include <geometry/bounds/boundingBox.h>
#include <geometry/intersection/intersection.h>
#include <math.h>
#include <stack>

namespace RenderLib {
namespace Geometry {
namespace Delaunay {

// the internal namespace hides away all implementation details to avoid exposing 
// them in the header, thus providing a much cleaner interface in the parent 
// Delaunay namespace

namespace internal { 
	// Foreward declarations /////////////////////////////////////////////////

	// Predicates //////////////////////////////////////////////////////////////////////////

	// Determines whether p is above the plane ( > 0 ), below ( < 0 ) or on the plane ( = 0 )
	REAL orient( const Point& a, 
		const Point& b, 
		const Point& c, 
		const Point& p );

	// returns > 0 if p is inside the sphere described by A,B,C,D, < 0 outside, = 0 on the sphere
	REAL inSphere( const Point& a, 
		const Point& b, 
		const Point& c, 
		const Point& d, 
		const Point& p );

	// Is p inside tetrahedron t?
	bool inside( const Point& p, const tetrahedron_t& t, const CoreLib::List< Point >& vertices );

	// whether 4 points are coplanar
	bool coplanar( const Point& a, const Point& b, const Point& c, const Point& d );

	// Bistellar flips /////////////////////////////////////////////////////////////////////

	void flip14( const unsigned int pointIndex, const unsigned int tetrahedron, 
		CoreLib::List< tetrahedron_t >& tetrahedra,
		const CoreLib::List< Point >& vertices,
		unsigned int resultingTetrahedra[4] );

	bool flip23( const unsigned int tetrahedron1, const unsigned int tetrahedron2, 							
		CoreLib::List< tetrahedron_t >& tetrahedra,
		const CoreLib::List< Point >& vertices,
		unsigned int resultingTetrahedra[3] );

	void flip32( const unsigned int tetrahedron1, const unsigned int tetrahedron2, const unsigned int tetrahedron3, 							
		CoreLib::List< tetrahedron_t >& tetrahedra,
		const CoreLib::List< Point >& vertices,
		unsigned int resultingTetrahedra[2] );

	void flip44( const unsigned int tetrahedron1, const unsigned int tetrahedron2, const unsigned int tetrahedron3, const unsigned int tetrahedron4,							
		CoreLib::List< tetrahedron_t >& tetrahedra,
		const CoreLib::List< Point >& vertices,
		unsigned int resultingTetrahedra[4] );

	// Auxiliary operations ////////////////////////////////////////////////////////////////

	void containingTetrahedron( const RenderLib::Math::Point3f& center, const float radius, // circumsphere  
		tetrahedron_t& t, CoreLib::List< Point >& points );

	// Given a tetrahedron and a face, ensures the potential adjacent tetrahedron sharing
	// that face points to the given tetrahedron.
	void adjustNeighborVicinity( const int iT, const int f, CoreLib::List< tetrahedron_t >& tetrahedra );

	// returns the index of the tetrahedron containing p, or -1 if not found. 
	// It retrieves the result by searching from a given sourceT tetrahedron index. 
	int walk( const Point& p, const int sourceT, const CoreLib::List< Point >& vertices, const CoreLib::List< tetrahedron_t >& tetrahedra );

	// Flips two given tetrahedra: T and Ta, which are non-Delaunay, 
	// into a Delaunay configuration using bistellar flips
	void flip(  const int T, const int Ta, const int p,
		const CoreLib::List< Point >& vertices,
		CoreLib::List< tetrahedron_t >& tetrahedra,
		std::stack< unsigned int >& needTesting );

	void insertOnePoint( const CoreLib::List< Point >& vertices, const int pointIndex, 
		CoreLib::List< tetrahedron_t >& tetrahedra );

	//////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////
	// tetrahedron_t
	//////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////

	namespace Tetrahedron { // all operations related to tetrahedron_t

		//////////////////////////////////////////////////////////////////////
		// Forward declarations //////////////////////////////////////////////
		//////////////////////////////////////////////////////////////////////

		void markInvalid( tetrahedron_t& t );
		REAL getFaceArea( const tetrahedron_t& t, const int f, const CoreLib::List< Point >& vertices );
		int getFaceFromVertices( const tetrahedron_t& t, const int a, const int b, const int c );
		int getVertexOutsideFace( const tetrahedron_t& t, int f );
		bool sameWinding( const int v1[3], const int v2[3] );
		int sharedFace( const tetrahedron_t& t, const tetrahedron_t& other, bool reversed );
		bool adjacentTo( const tetrahedron_t& t, const tetrahedron_t& other );
		bool checkNeighbors( const tetrahedron_t& t, const int thisIndex, const CoreLib::List< tetrahedron_t >& tetrahedra, const CoreLib::List< Point >& vertices );
		void destroy( tetrahedron_t& t, CoreLib::List< tetrahedron_t >& tetrahedra );
		bool sameOrientation( const tetrahedron_t& t, const int face, const tetrahedron_t& other, const int otherFace, const CoreLib::List< Point >& vertices );
		void reverseFace( tetrahedron_t& t, const int f );
		bool isFlat( const tetrahedron_t& t, const CoreLib::List< Point >& vertices );
		void fixFaceOrientations( tetrahedron_t& t, const CoreLib::List< Point >& vertices );
		bool checkFaceOrientations( const tetrahedron_t& t, const CoreLib::List< Point >& vertices );	

		//////////////////////////////////////////////////////////////////////

		bool checkFaceOrientations( const tetrahedron_t& T, 
									const CoreLib::List< Point >& vertices ) {
				
			// check whether the tetrahedron centroid lies behind every face

			const Point& vo = vertices[ T.v[ 0 ] ];
			const Point& va = vertices[ T.v[ 1 ] ];
			const Point& vb = vertices[ T.v[ 2 ] ];
			const Point& vc = vertices[ T.v[ 3 ] ];
#if 0 // using centroid
			const Vector3<REAL> a = va - vo;
			const Vector3<REAL> b = vb - vo;
			const Vector3<REAL> c = vc - vo;
			Point C = vo + ( a + b + c ) / 4;
#elif 1 // using incenter
			const REAL a = getFaceArea( T, 2, vertices );
			const REAL b = getFaceArea( T, 3, vertices );
			const REAL c = getFaceArea( T, 1, vertices );
			const REAL d = getFaceArea( T, 0, vertices );
			const REAL t = a + b + c + d;
			Point C = vo * ( a / t ) + va * ( b / t ) + vb * ( c / t ) + vc * ( d / t);
#else // monge point
			const Vector3<REAL> a = va - vo;
			const Vector3<REAL> b = vb - vo;
			const Vector3<REAL> c = vc - vo;
			const Vector3<REAL> bCrossC = Vector3<REAL>::cross( b, c );
			const Vector3<REAL> cCrossA = Vector3<REAL>::cross( c, a );
			const Vector3<REAL> aCrossB = Vector3<REAL>::cross( a, b );

			Point C = vo + ( a * ( ( b + c ) * bCrossC ) + b * ( ( c + a ) * cCrossA ) + c * ( ( a + b ) * aCrossB ) ) / ( a * bCrossC * 2 );

#endif
			int v2[3];
			for( int i = 0; i < 4; i++ ) {
				T.getFaceVertices( i, v2[0], v2[1], v2[2] );	
				if( orient( vertices[ v2[0] ], vertices[ v2[1] ], vertices[ v2[2] ], C ) > 0 ) {
					return false;
				}
			}
			return true;
		}

		//////////////////////////////////////////////////////////////////////////
		// isFlat:
		// A tetrahedron is flat if its 4 vertices lie in the same plane
		// A flat tetrahedron has no circumsphere
		//////////////////////////////////////////////////////////////////////////
		bool isFlat( const tetrahedron_t& t, const CoreLib::List< Point >& vertices ) {
			return coplanar( vertices[ t.v[ 0 ] ], vertices[ t.v[ 1 ] ], vertices[ t.v[ 2 ] ], vertices[ t.v[ 3 ] ] );
		}

		//////////////////////////////////////////////////////////////////////////
		// fixFaceOrientations:
		// ensure the tetrahedron centroid lies behind every face
		//////////////////////////////////////////////////////////////////////////
		void fixFaceOrientations( tetrahedron_t& t, const CoreLib::List< Point >& vertices ) {
			using namespace RenderLib::Math;

			// check whether the tetrahedron centroid lies behind every face

			const Point& vo = vertices[ t.v[ 0 ] ];
			const Point& va = vertices[ t.v[ 1 ] ];
			const Point& vb = vertices[ t.v[ 2 ] ];
			const Point& vc = vertices[ t.v[ 3 ] ];

			if ( isFlat( t, vertices ) ) {
				// the test makes no sense on flat tetrahedra as the centroid is neither inside nor outside the tetrahedron
				return;
			}
#if 1 // using centroid
			const Vector3<REAL> a = va - vo;
			const Vector3<REAL> b = vb - vo;
			const Vector3<REAL> c = vc - vo;
			Point C = vo + ( a + b + c ) / 4;
#elif 1 // using incenter
			const REAL a = getFaceArea( t, 2, vertices );
			const REAL b = getFaceArea( t, 3, vertices );
			const REAL c = getFaceArea( t, 1, vertices );
			const REAL d = getFaceArea( t, 0, vertices );
			const REAL t = a + b + c + d;
			Point C = vo * ( a / t ) + va * ( b / t ) + vb * ( c / t ) + vc * ( d / t);
#else // monge point
			const Vector3<REAL> a = va - vo;
			const Vector3<REAL> b = vb - vo;
			const Vector3<REAL> c = vc - vo;
			const Vector3<REAL> bCrossC = Vector3<REAL>::cross( b, c );
			const Vector3<REAL> cCrossA = Vector3<REAL>::cross( c, a );
			const Vector3<REAL> aCrossB = Vector3<REAL>::cross( a, b );

			Point C = vo + ( a * ( ( b + c ) * bCrossC ) + b * ( ( c + a ) * cCrossA ) + c * ( ( a + b ) * aCrossB ) ) / ( a * bCrossC * 2 );

#endif
			int v2[3];
			for( int i = 0; i < 4; i++ ) {
				t.getFaceVertices( i, v2[0], v2[1], v2[2] );
				const Vector3<REAL> faceNormal = Vector3<REAL>::normalize( Vector3<REAL>::cross( vertices[ v2[2] ] - vertices[ v2[0] ], vertices[ v2[1] ] - vertices[ v2[0] ] ) ); // we can get away not normalizing it, since we're just interested in the distance sign
				const REAL d = Vector3<REAL>::dot(faceNormal, vertices[ v2[0] ].fromOrigin());
				const REAL distToPlane = Vector3<REAL>::dot(faceNormal, C.fromOrigin()) - d;
				const REAL epsilon = 1e-1;
				if ( distToPlane > epsilon ) { // the centroid should be behind the plane
					reverseFace( t, i ); 
				}
			}				
		}

		void markInvalid( tetrahedron_t& t ) {
			for( int i = 0; i < 4; i++ ) {
				t.v[ i ] = -1;
				t.neighbors[ i ] = -1;
			}
			t.face[ 0 ][ 0 ] = 0; t.face[ 0 ][ 1 ] = 1; t.face[ 0 ][ 2 ] = 2;
			t.face[ 1 ][ 0 ] = 0; t.face[ 1 ][ 1 ] = 3; t.face[ 1 ][ 2 ] = 1;
			t.face[ 2 ][ 0 ] = 1; t.face[ 2 ][ 1 ] = 3; t.face[ 2 ][ 2 ] = 2;
			t.face[ 3 ][ 0 ] = 2; t.face[ 3 ][ 1 ] = 3; t.face[ 3 ][ 2 ] = 0;
		}

		REAL getFaceArea( const tetrahedron_t& t, const int f, const CoreLib::List< Point >& vertices ) {
			using namespace RenderLib::Geometry;
			switch( f ) {
					case 0:
						return triangleArea( vertices[ t.v[0] ], vertices[ t.v[1] ], vertices[ t.v[2] ] );
					case 1:
						return triangleArea( vertices[ t.v[0] ], vertices[ t.v[3] ], vertices[ t.v[1] ] );
					case 2:
						return triangleArea( vertices[ t.v[1] ], vertices[ t.v[3] ], vertices[ t.v[2] ] );
					case 3:
						return triangleArea( vertices[ t.v[2] ], vertices[ t.v[3] ], vertices[ t.v[0] ] );
					default: 
						assert( false );
						return -1;
			}
		}

		int getFaceFromVertices( const tetrahedron_t& t, const int a, const int b, const int c ) {
			for( int i = 0; i < 4; i++ ) {
				int check = 0;
				for( int j = 0; j < 3; j++ ) {
					if ( t.v[ t.face[ i ][ j ] ] == a || 
						 t.v[ t.face[ i ][ j ] ] == b || 
						 t.v[ t.face[ i ][ j ] ] == c ) {
						check |= ( 1 << j );						
					}
				}
				if ( check == 7 /* 111 */ ) {
					return i;
				}
			}

			return -1;
		}

		int getVertexOutsideFace( const tetrahedron_t& t, int f ) {
			assert( f >= 0 && f < 4 );
			int check = 0;
			for( int i = 0; i < 3; i++ ) {
				check |= ( 1 << t.face[ f ][ i ] );
			}
			switch( check ) {
					case 14: // 1110
						return t.v[0];
					case 13: // 1101
						return t.v[1];
					case 11: // 1011
						return t.v[2];
					case 7: // 0111
						return t.v[3];
					default:
						assert( false );
						return -1;
			}
		}

		////////////////////////////////////////////////////////////////////////
		// sameWinding:
		// We use this function to check whether two faces sharing the same vertices are reversed
		// without needing to calculate the normal
		////////////////////////////////////////////////////////////////////////
		bool sameWinding( const int v1[3], const int v2[3] ) {
			int offset2;
			for( offset2 = 0; offset2 < 3; offset2++ ) {
				if ( v2[ offset2 ] == v1[ 0 ] ) {
					break;
				}
			}
			if ( offset2 == 3 ) { 
				// sequences are not even the same
				return false;
			}
			for( int offset1 = 1; offset1 < 3; offset1++ ) {
				offset2 = ( offset2 + 1 ) % 3;
				if ( v1[ offset1 ] != v2[ offset2 ] ) {
					return false;
				}
			}
			return true;
		}

		int sharedFace( const tetrahedron_t& t, const tetrahedron_t& other, bool reversed ) {
			int verts[ 3 ];
			int verts2[3];
			for( int i = 0; i < 4; i++ ) {
				other.getFaceVertices( i, verts[ 0 ], verts[ 1 ], verts[ 2 ] );
				int face = getFaceFromVertices( t, verts[ 0 ], verts[ 2 ], verts[ 1 ] ); // reverse the face order as they're confronted
				if ( face >= 0 ) {
					t.getFaceVertices( face, verts2[ 0 ], verts2[ 1 ], verts2[ 2 ] );

					if ( reversed &&  // tetrahedra are adjacent on each side of the adjacent face, so the face is declared on reverse order for each tetrahedron 
						sameWinding( verts, verts2 ) ) {
							face = -1;					
					} 
					if ( !reversed && // the tetrahedra overlap on the same space (during flip 44 for example) so the shared faces have the same orientation
						!sameWinding( verts, verts2 ) ) {
							face = -1;					
					}
					if ( face >= 0 ) {
						return face;
					}
				}
			}

			return -1;
		}

		bool adjacentTo( const tetrahedron_t& t, const tetrahedron_t& other ) {
			return sharedFace( t, other, true ) >= 0;
		}

		bool checkNeighbors( const tetrahedron_t& t, const int thisIndex, const CoreLib::List< tetrahedron_t >& tetrahedra, const CoreLib::List< Point >& vertices ) {
			for( int i = 0; i < 4; i++ ) {
				if ( t.neighbors[ i ] >= 0 ) {
					const tetrahedron_t& neighbor = tetrahedra[ t.neighbors[ i ] ];
					if ( !neighbor.isValid() ) {
						return false;
					}

					const int sf = sharedFace( neighbor, t, true );
					if ( sf < 0 ) {
						return false;
					}

					if ( sameOrientation( t, i, neighbor, sf, vertices ) ) { // they should be reversed
						return false;
					}

					if ( neighbor.neighbors[ sf ] != thisIndex ) {
						return false;
					}
				}
			}
			return true;
		}

		void destroy( tetrahedron_t& t, CoreLib::List< tetrahedron_t >& tetrahedra ) {
			// unlink neighbors
			for( int i = 0; i < 4; i++ ) {
				if ( t.neighbors[ i ] >= 0 ) {
					tetrahedron_t& n = tetrahedra[ t.neighbors[ i ] ];
					if ( !n.isValid() ) {
						continue;
					}
					const int sf = sharedFace( n, t, true );
					n.neighbors[ sf ] = -1;
				}
			}

			markInvalid(t);
		}

		////////////////////////////////////////////////////////////////////////
		// sameOrientation:
		// checks whether 'face' has the same orientation than 'otherFace' 
		// (normals point toward the same direction)
		////////////////////////////////////////////////////////////////////////
		bool sameOrientation( const tetrahedron_t& t, const int face, const tetrahedron_t& other, const int otherFace, const CoreLib::List< Point >& vertices ) {
			using namespace RenderLib::Math;
			int vi1[3], vi2[3];
			t.getFaceVertices( face, vi1[0], vi1[1], vi1[2] );
			other.getFaceVertices( otherFace, vi2[0], vi2[1], vi2[2] );
			Vector3<REAL> n1 = Vector3<REAL>::cross( vertices[vi1[1]] - vertices[vi1[0]], vertices[vi1[2]] - vertices[vi1[0]] );
			Vector3<REAL> n2 = Vector3<REAL>::cross( vertices[vi2[1]] - vertices[vi2[0]], vertices[vi2[2]] - vertices[vi2[0]] );
			return Vector3<REAL>::dot(n1, n2) > (REAL)0;
		}

		void reverseFace( tetrahedron_t& t, const int f ) {
			const int v0 = t.face[ f ][ 0 ];
			t.face[ f ][ 0 ] = t.face[ f ][ 2 ];
			t.face[ f ][ 2 ] = v0;
		}

	} // namespace Tetrahedron

	

	//////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////
	// Delaunay3D
	//////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// Predicates
	//////////////////////////////////////////////////////////////////////////

	/*
	================
	Delaunay3D::Orient

	Determines whether p is above ( > 0 ) the plane defined by A,B,C, 
	below ( < 0 ) or on the plane ( = 0 )
	The test follows the left-hand rule, clockwise order = up
	================
	*/
	REAL orient( const Point& a, const Point& b, const Point& c, const Point& p ) {
		using namespace RenderLib::Math;
		Matrix4<REAL> M( a.x, a.y, a.z, 1,
						 b.x, b.y, b.z, 1, 
						 c.x, c.y, c.z, 1, 
						 p.x, p.y, p.z, 1 );
		
		const REAL epsilon = (REAL)0.001;
		const REAL det = M.determinant();

#if _DEBUG
		{
			Vector3<REAL> n = Vector3<REAL>::cross( c - a , b - a );
			if ( n.lengthSquared() > 1e-4 ) {
				n.normalize();
				const REAL d = Vector3<REAL>::dot(n, a.fromOrigin());
				const REAL distToPlane = Vector3<REAL>::dot(n, p.fromOrigin()) - d;
				if ( det > epsilon ) {
					assert( distToPlane > 0 );
				} else if ( det < -epsilon ) {
					assert( distToPlane < 0 );
				}
			}
		}
#endif

		if( det < -epsilon || det > epsilon ) {
			return det;
		} 
		return 0;
	}

	/*
	================
	Delaunay3D::InSphere

	Returns > 0 if p is inside the sphere described by a,b,c,d, 
	< 0 outside, = 0 on the sphere
	================
	*/
	REAL inSphere( const Point& a, const Point& b, const Point& c, const Point& d, const Point& p ) {
		using namespace RenderLib::Math;
		Matrix5<REAL> M( a.x, a.y, a.z, a.x * a.x + a.y * a.y + a.z * a.z, 1, 
						 b.x, b.y, b.z, b.x * b.x + b.y * b.y + b.z * b.z, 1, 
						 c.x, c.y, c.z, c.x * c.x + c.y * c.y + c.z * c.z, 1, 
						 d.x, d.y, d.z, d.x * d.x + d.y * d.y + d.z * d.z, 1, 
						 p.x, p.y, p.z, p.x * p.x + p.y * p.y + p.z * p.z, 1 );

		const REAL epsilon = (REAL)0.0001;
		const REAL det = M.determinant();

		assert( orient( a, b, c, d ) >= 0 );
#if _DEBUG
		{
			Point center; 
			REAL radius = 0;
			SphereFrom4Points<REAL>( a, b, c, d, center, radius );
			const REAL dist = center.distanceTo( p );

			if( det < -epsilon ) { 
				assert( dist > radius - epsilon );
			} else if ( det > epsilon ) {
				assert( dist < radius + epsilon );
			} 			
		}
#endif

		if( det < -epsilon || det > epsilon ) {
			return det;
		} 
		return 0;
	}

	/*
	================
	Delaunay3D::Inside

	Whether the point p is inside the tetrahedron t
	================
	*/
	bool inside( const Point& p, const tetrahedron_t& t, const CoreLib::List< Point >& vertices ) {
		
		using namespace RenderLib::Math;

		int a, b, c;
		if ( Tetrahedron::isFlat( t, vertices ) ) {
			return false;
		}

		for( int i = 0; i < 4; i++ ) {
			t.getFaceVertices( i, a, b, c );
			//if ( Orient( vertices[ a ], vertices[ b ], vertices[ c ], p ) > 0 ) {
			//	return false;
			//}
			const Vector3<REAL> n = Vector3<REAL>::normalize( Vector3<REAL>::cross( vertices[ c ] - vertices[ a ], vertices[ b ] - vertices[ a ] ) );
			const REAL d = Vector3<REAL>::dot(n, vertices[ a ].fromOrigin());
			const REAL distToPlane = Vector3<REAL>::dot(n, p.fromOrigin()) - d;
			if ( distToPlane > 1e-4 ) {
				return false;
			}
		}
		return true;
	}	

	/*
	================
	Delaunay3D::Coplanar

	Whether 4 points are coplanar
	================
	*/
	bool coplanar( const Point& a, const Point& b, const Point& c, const Point& d ) {
		return orient( a, b, c, d ) == 0;
	}

	//////////////////////////////////////////////////////////////////////////
	// Bistellar Flips
	//////////////////////////////////////////////////////////////////////////

	/*
	================
	Delaunay3D::Flip14

	Bistellar Flip 1->4 : inserts the point given by pointIndex into 
	the tetrahedron splitting it into 4 new adjacent tetrahedrons
	================
	*/
	void flip14( const unsigned int pointIndex, const unsigned int tetrahedron, 
		CoreLib::List< tetrahedron_t >& tetrahedra,
		const CoreLib::List< Point >& vertices,
		unsigned int resultingTetrahedra[4] ) {

		tetrahedron_t srcT = tetrahedra[ (int)tetrahedron ];
		assert( Tetrahedron::checkFaceOrientations( srcT, vertices ) );

		assert( Tetrahedron::checkNeighbors( srcT, tetrahedron, tetrahedra, vertices ) );

#if _DEBUG
		for( int i = 0; i < 4; i++ ) {
			assert( srcT.v[ i ] != (int)pointIndex );
		}
#endif

		assert( inside( vertices[ pointIndex ], srcT, vertices ) );

		tetrahedra.preAllocate( tetrahedra.size() + 3 ); // ensure the list is not resized while we're holding references to its elements

		const unsigned int iResT1 = tetrahedron; // reuse the source tetrahedron slot
		tetrahedron_t& resT1 = tetrahedra[ iResT1 ];
		Tetrahedron::destroy( resT1, tetrahedra );
		const unsigned int iResT2 = (unsigned int)tetrahedra.size(); 
		tetrahedron_t& resT2 = tetrahedra.append();
		const unsigned int iResT3 = (unsigned int)tetrahedra.size(); 
		tetrahedron_t& resT3 = tetrahedra.append();
		const unsigned int iResT4 = (unsigned int)tetrahedra.size(); 
		tetrahedron_t& resT4 = tetrahedra.append();

		assert( !resT1.isValid() );
		assert( !resT2.isValid() );
		assert( !resT3.isValid() );
		assert( !resT4.isValid() );

		resultingTetrahedra[ 0 ] = iResT1;
		resultingTetrahedra[ 1 ] = iResT2;
		resultingTetrahedra[ 2 ] = iResT3;
		resultingTetrahedra[ 3 ] = iResT4;

		// res1
		srcT.getFaceVertices( 0, resT1.v[0], resT1.v[1], resT1.v[2] );
		resT1.v[ 3 ] = pointIndex;
		Tetrahedron::fixFaceOrientations( resT1, vertices );
		assert( Tetrahedron::sameOrientation( srcT, 0, resT1, 0, vertices ) );

		// res2
		srcT.getFaceVertices( 1, resT2.v[0], resT2.v[1], resT2.v[2] );
		resT2.v[ 3 ] = pointIndex;
		Tetrahedron::fixFaceOrientations( resT2, vertices );		
		assert( Tetrahedron::sameOrientation( srcT, 1, resT2, 0, vertices ) );
	
		// res3
		srcT.getFaceVertices( 2, resT3.v[0], resT3.v[1], resT3.v[2] );
		resT3.v[ 3 ] = pointIndex;
		Tetrahedron::fixFaceOrientations( resT3, vertices );
		assert( Tetrahedron::sameOrientation( srcT, 2, resT3, 0, vertices ) );
		
		// res4
		srcT.getFaceVertices( 3, resT4.v[0], resT4.v[1], resT4.v[2] );
		resT4.v[ 3 ] = pointIndex;
		Tetrahedron::fixFaceOrientations( resT4, vertices );
		assert( Tetrahedron::sameOrientation( srcT, 3, resT4, 0, vertices ) );

		// Adjust neighbors 

		resT1.neighbors[ 0 ] = srcT.neighbors[ 0 ];
		resT1.neighbors[ Tetrahedron::sharedFace( resT1, resT2, true ) ] = iResT2;
		resT1.neighbors[ Tetrahedron::sharedFace( resT1, resT3, true ) ] = iResT3;
		resT1.neighbors[ Tetrahedron::sharedFace( resT1, resT4, true ) ] = iResT4;

		resT2.neighbors[ 0 ] = srcT.neighbors[ 1 ];
		resT2.neighbors[ Tetrahedron::sharedFace( resT2, resT4, true ) ] = iResT4;
		resT2.neighbors[ Tetrahedron::sharedFace( resT2, resT3, true ) ] = iResT3;
		resT2.neighbors[ Tetrahedron::sharedFace( resT2, resT1, true ) ] = iResT1;

		resT3.neighbors[ 0 ] = srcT.neighbors[ 2 ];
		resT3.neighbors[ Tetrahedron::sharedFace( resT3, resT2, true ) ] = iResT2;
		resT3.neighbors[ Tetrahedron::sharedFace( resT3, resT4, true ) ] = iResT4;
		resT3.neighbors[ Tetrahedron::sharedFace( resT3, resT1, true ) ] = iResT1;

		resT4.neighbors[ 0 ] = srcT.neighbors[ 3 ];
		resT4.neighbors[ Tetrahedron::sharedFace( resT4, resT3, true ) ] = iResT3;
		resT4.neighbors[ Tetrahedron::sharedFace( resT4, resT2, true ) ] = iResT2;
		resT4.neighbors[ Tetrahedron::sharedFace( resT4, resT1, true ) ] = iResT1;
		
		for( int i = 0; i < 4; i++ ) {
			adjustNeighborVicinity( resultingTetrahedra[ i ], 0, tetrahedra );
		}
#if _DEBUG
		for( int i = 0; i < 4; i++ ) {
			assert( Tetrahedron::checkNeighbors( tetrahedra[ resultingTetrahedra[ i ] ], resultingTetrahedra[ i ], tetrahedra, vertices ) );
		}
#endif
	}

	/*
	================
	Delaunay3D::Flip23

	Takes two adjacent tetrahedra [abcd][bcde] sharing a face bcd, and splits them into 3 new
	tetrahedra linked by a single edge joining a-e
	================
	*/
	bool flip23( const unsigned int tetrahedron1, const unsigned int tetrahedron2, 
				 CoreLib::List< tetrahedron_t >& tetrahedra,
				 const CoreLib::List< Point >& vertices,
				 unsigned int resultingTetrahedra[3] ) {

		tetrahedron_t srcT1 = tetrahedra[ tetrahedron1 ];
		tetrahedron_t srcT2 = tetrahedra[ tetrahedron2 ];

		assert( Tetrahedron::checkFaceOrientations( srcT1, vertices ) );
		assert( Tetrahedron::checkFaceOrientations( srcT2, vertices ) );

		assert( Tetrahedron::checkNeighbors( srcT1, tetrahedron1, tetrahedra, vertices ) );
		assert( Tetrahedron::checkNeighbors( srcT2, tetrahedron2, tetrahedra, vertices ) );

		assert( Tetrahedron::adjacentTo( srcT1, srcT2 ) );
				
		// The resulting 3 tetrahedra will all be adjacent to an edge linking the
		// non-shared vertices of the 2 source tetrahedra
		
		const int sharedFaceT1 = Tetrahedron::sharedFace( srcT1, srcT2, true ); // the shared face's index according to T1
		const int sharedFaceT2 = Tetrahedron::sharedFace( srcT2, srcT1, true ); // the shared face's index according to T2
																				// note that the shared face is the same geometrically
																				// but we're referring to it by its index in 
																				// each tetrahedron's particular array

		const int a = Tetrahedron::getVertexOutsideFace( srcT1, sharedFaceT1 );
		const int e = Tetrahedron::getVertexOutsideFace( srcT2, sharedFaceT2 );
		
		int b,c,d;
		srcT1.getFaceVertices( sharedFaceT1, b, c, d );

		// vertices A and E can be the same in a degenerate Case 1, where two adjacent tetrahedra also share
		// the vertex opposed to their shared face (that is, they're effectively the same tetrahedron with the 
		// faces declared in a different order). This can happen after inserting a vertex in the face of an existing
		// tetrahedron and causing N chained Case 4 till we close the cycle and reach the original point again.
		// This degenerate Case1 will generate 3 new (smaller) flat tetrahedra, that should eventually go away
		// with further flips
		// ! don't --> assert( a != e );
		assert( a != b && a != c && a != d );

		// assert outside vertex from T2 lies inside the sphere described by t1 (premise for a flip23)
		// the InSphere test requires points ABCD to be ordered such as D is in front of the plane defined
		// by ABC (clockwise order). Therefore since we've obtained our plane points from the tetrahedron
		// face, which points outwards, we'll need to invert them to make the 4th point be above the plane.

		assert( Tetrahedron::isFlat( srcT1, vertices ) || 
				inSphere(	vertices[ b ], vertices[ d ], vertices[ c ], // note inverted d,c
							vertices[ a ], // srcT1 4th vertex, above the inversed sharedFaceT1
							vertices[ e ]  // srcT2 4th vertex, which should be behind our tested plane
						 ) >= 0 
				); 

		// flip 23 is only possible if the edge a-e crosses the shared face
		// nevertheless on a Case 4, the segment a-e is going to be right upon one
		// of the edges from the face b,c,d, and we'll run into numeric issues.
		// Since we've already performed this check outside the function, we assume 
		// we know what we're doing here													 
		//REAL t,u,v;
		//if ( !SegmentTriangleIntersect_DoubleSided( vertices[ a ], vertices[ e ], vertices[ b ], vertices[ c ], vertices[ d ], t, u, v ) ) {
		//	return false;
		//}

		tetrahedra.preAllocate( tetrahedra.size() + 1 ); // ensure the list is not resized while we're holding references to its elements

		const unsigned int iResT1 = tetrahedron1; // reuse the source tetrahedron1 slot
		tetrahedron_t& resT1 = tetrahedra[ iResT1 ];
		Tetrahedron::destroy( resT1, tetrahedra );
		const unsigned int iResT2 = tetrahedron2; // reuse the source tetrahedron2 slot
		tetrahedron_t& resT2 = tetrahedra[ iResT2 ];
		Tetrahedron::destroy( resT2, tetrahedra );
		const unsigned int iResT3 = (unsigned int)tetrahedra.size(); 
		tetrahedron_t& resT3 = tetrahedra.append();

		assert( !resT1.isValid() );
		assert( !resT2.isValid() );
		assert( !resT3.isValid() );

		resultingTetrahedra[ 0 ] = iResT1;
		resultingTetrahedra[ 1 ] = iResT2;
		resultingTetrahedra[ 2 ] = iResT3;

		
		// Result 1

		resT1.v[ 0 ] = b;
		resT1.v[ 1 ] = d;
		resT1.v[ 2 ] = a;
		resT1.v[ 3 ] = e;
		Tetrahedron::fixFaceOrientations( resT1, vertices );

		{ // adjust boundary faces' neighborhood
			
			// get the external faces in the new tetrahedron...
			const int bFace1 = Tetrahedron::getFaceFromVertices( resT1, a, b, d );
			const int bFace2 = Tetrahedron::getFaceFromVertices( resT1, b, e, d );

			// ...and the corresponding faces in the source tetrahedra
			const int bFace1Src = Tetrahedron::getFaceFromVertices( srcT1, a, b, d );
			const int bFace2Src = Tetrahedron::getFaceFromVertices( srcT2, b, e, d );

			assert( bFace1 >= 0 && bFace2 >= 0 && bFace1Src >= 0 && bFace2Src >= 0 );
			assert( Tetrahedron::isFlat( resT1, vertices ) || Tetrahedron::sameOrientation( srcT1, bFace1Src, resT1, bFace1, vertices ) );
			assert( Tetrahedron::isFlat( resT1, vertices ) || Tetrahedron::sameOrientation( srcT2, bFace2Src, resT1, bFace2, vertices ) );

			// copy the neighbors
			resT1.neighbors[ bFace1 ] = srcT1.neighbors[ bFace1Src ];
			resT1.neighbors[ bFace2 ] = srcT2.neighbors[ bFace2Src ];

			// and adjust the neighborhood
			adjustNeighborVicinity( iResT1, bFace1, tetrahedra );
			adjustNeighborVicinity( iResT1, bFace2, tetrahedra );		
		}

		// Result 2

		resT2.v[ 0 ] = d;
		resT2.v[ 1 ] = c;
		resT2.v[ 2 ] = a;
		resT2.v[ 3 ] = e;
		Tetrahedron::fixFaceOrientations( resT2, vertices );


		{ // adjust boundary faces' neighborhood

			// get the external faces in the new tetrahedron...
			const int bFace1 = Tetrahedron::getFaceFromVertices( resT2, c, a, d );
			const int bFace2 = Tetrahedron::getFaceFromVertices( resT2, d, e, c );

			// ...and the corresponding faces in the source tetrahedra
			const int bFace1Src = Tetrahedron::getFaceFromVertices( srcT1, c, a, d );
			const int bFace2Src = Tetrahedron::getFaceFromVertices( srcT2, d, e, c );

			assert( bFace1 >= 0 && bFace2 >= 0 && bFace1Src >= 0 && bFace2Src >= 0 );
			assert( Tetrahedron::isFlat( resT2, vertices ) || Tetrahedron::sameOrientation( srcT1, bFace1Src, resT2, bFace1, vertices ) );
			assert( Tetrahedron::isFlat( resT2, vertices ) || Tetrahedron::sameOrientation( srcT2, bFace2Src, resT2, bFace2, vertices ) );

			// copy the neighbors
			resT2.neighbors[ bFace1 ] = srcT1.neighbors[ bFace1Src ];
			resT2.neighbors[ bFace2 ] = srcT2.neighbors[ bFace2Src ];

			// and adjust the neighborhood
			adjustNeighborVicinity( iResT2, bFace1, tetrahedra );
			adjustNeighborVicinity( iResT2, bFace2, tetrahedra );
		}

		// Result 3

		resT3.v[ 0 ] = c;
		resT3.v[ 1 ] = b;
		resT3.v[ 2 ] = a;
		resT3.v[ 3 ] = e;
		Tetrahedron::fixFaceOrientations( resT3, vertices );

		{ // adjust boundary faces' neighborhood

			// get the external faces in the new tetrahedron...
			const int bFace1 = Tetrahedron::getFaceFromVertices( resT3, b, a, c );
			const int bFace2 = Tetrahedron::getFaceFromVertices( resT3, c, e, b );

			// ...and the corresponding faces in the source tetrahedra
			const int bFace1Src = Tetrahedron::getFaceFromVertices( srcT1, b, a, c );
			const int bFace2Src = Tetrahedron::getFaceFromVertices( srcT2, c, e, b );

			assert( bFace1 >= 0 && bFace2 >= 0 && bFace1Src >= 0 && bFace2Src >= 0 );
			assert( Tetrahedron::isFlat( resT3, vertices ) || Tetrahedron::sameOrientation( srcT1, bFace1Src, resT3, bFace1, vertices ) );
			assert( Tetrahedron::isFlat( resT3, vertices ) || Tetrahedron::sameOrientation( srcT2, bFace2Src, resT3, bFace2, vertices ) );

			// copy the neighbors
			resT3.neighbors[ bFace1 ] = srcT1.neighbors[ bFace1Src ];
			resT3.neighbors[ bFace2 ] = srcT2.neighbors[ bFace2Src ];

			// and adjust the neighborhood
			adjustNeighborVicinity( iResT3, bFace1, tetrahedra );
			adjustNeighborVicinity( iResT3, bFace2, tetrahedra );
		}

		// adjust internal neighborhood
		resT1.neighbors[ Tetrahedron::sharedFace( resT1, resT2, true ) ] = iResT2;
		resT1.neighbors[ Tetrahedron::sharedFace( resT1, resT3, true ) ] = iResT3;
		resT2.neighbors[ Tetrahedron::sharedFace( resT2, resT1, true ) ] = iResT1;
		resT2.neighbors[ Tetrahedron::sharedFace( resT2, resT3, true ) ] = iResT3;
		resT3.neighbors[ Tetrahedron::sharedFace( resT3, resT1, true ) ] = iResT1;
		resT3.neighbors[ Tetrahedron::sharedFace( resT3, resT2, true ) ] = iResT2;

		
#if _DEBUG
		for( int i = 0; i < 3; i++ ) {
			assert( Tetrahedron::checkNeighbors( tetrahedra[ resultingTetrahedra[ i ] ], resultingTetrahedra[ i ], tetrahedra, vertices ) );
		}
#endif
		return true;
	}

	int intCompare( const void* p0, const void* p1 );

	/*
	================
	Delaunay3D::Flip32

	Flips 3 adjacent tetrahedra into 2. The function reuses the two first slots in
	the tetrahedra array, but the later one given by tetrahedron3 must be marked as
	invalid.
	================
	*/
	void flip32( const unsigned int tetrahedron1, const unsigned int tetrahedron2, const unsigned int tetrahedron3, 
				 CoreLib::List< tetrahedron_t >& tetrahedra,
				 const CoreLib::List< Point >& vertices,
				 unsigned int resultingTetrahedra[2] ) {

		using namespace RenderLib::Math;

		tetrahedron_t srcT1 = tetrahedra[ tetrahedron1 ];
		tetrahedron_t srcT2 = tetrahedra[ tetrahedron2 ];
		tetrahedron_t srcT3 = tetrahedra[ tetrahedron3 ];

		assert( Tetrahedron::checkFaceOrientations( srcT1, vertices ) );
		assert( Tetrahedron::checkFaceOrientations( srcT2, vertices ) );
		assert( Tetrahedron::checkFaceOrientations( srcT3, vertices ) );

		assert( Tetrahedron::checkNeighbors( srcT1, tetrahedron1, tetrahedra, vertices ) );
		assert( Tetrahedron::checkNeighbors( srcT2, tetrahedron2, tetrahedra, vertices ) );
		assert( Tetrahedron::checkNeighbors( srcT3, tetrahedron3, tetrahedra, vertices ) );

		const unsigned int iResT1 = tetrahedron1; // reuse the source tetrahedron1 slot
		tetrahedron_t& resT1 = tetrahedra[ iResT1 ];
		Tetrahedron::destroy( resT1, tetrahedra );
		const unsigned int iResT2 = tetrahedron2; // reuse the source tetrahedron2 slot
		tetrahedron_t& resT2 = tetrahedra[ iResT2 ];
		Tetrahedron::destroy( resT2, tetrahedra );

		assert( !resT1.isValid() );
		assert( !resT2.isValid() );

		resultingTetrahedra[ 0 ] = iResT1;
		resultingTetrahedra[ 1 ] = iResT2;

		// Source tetrahedra: [abdp][bcdp][cadp]
		// resulting tetrahedra: [abcd][abcp]

		int a = -1, b = -1, c = -1, d = -1, p = -1;

		{		
			const int sharedFace12 = Tetrahedron::sharedFace( srcT1, srcT2, true );
			const int sharedFace13 = Tetrahedron::sharedFace( srcT1, srcT3, true );
			const int sharedFace23 = Tetrahedron::sharedFace( srcT2, srcT3, true );
			assert( sharedFace12 >= 0 && sharedFace13 >= 0 && sharedFace23 >= 0 );

			int sharedFaceVertices[ 9 ];
			srcT1.getFaceVertices( sharedFace12, sharedFaceVertices[ 3 * 0 + 0 ], sharedFaceVertices[ 3 * 0 + 1 ], sharedFaceVertices[ 3 * 0 + 2 ] );
			srcT1.getFaceVertices( sharedFace13, sharedFaceVertices[ 3 * 1 + 0 ], sharedFaceVertices[ 3 * 1 + 1 ], sharedFaceVertices[ 3 * 1 + 2 ] );
			srcT2.getFaceVertices( sharedFace23, sharedFaceVertices[ 3 * 2 + 0 ], sharedFaceVertices[ 3 * 2 + 1 ], sharedFaceVertices[ 3 * 2 + 2 ] );

			// These shared faces should all have 2 vertices in common (d and p) and 
			// the remaining vertex only shared with 1 other face (a, b, c)
			qsort( sharedFaceVertices, 9, sizeof( int ), intCompare );

			int uniqueVertices[5];
			int occurrence[5];
			memset( occurrence, 0, 5 * sizeof(int) );
			int uv = 1;
			uniqueVertices[ 0 ] = sharedFaceVertices[ 0 ];
			occurrence[ 0 ] = 1;
			for( int i = 1; i < 9; i++ ) {
				bool found = false;
				for( int j = 0; j < uv; j++ ) {
					if( uniqueVertices[ j ] == sharedFaceVertices[ i ] ) {
						occurrence[ j ]++;
						found = true;
						break;
					}
				}
				if ( !found ) {
					uniqueVertices[ uv ] = sharedFaceVertices[ i ];
					occurrence[ uv ] = 1;
					uv ++;
				}
			}

			assert( uv == 5 );

			int dp[2];
			int abc[3];
			int iDp = 0;
			int iAbc = 0;
			for( int i = 0; i < 5; i++ ) {
				if( occurrence[ i ] == 3 ) {
					dp[ iDp++ ] = uniqueVertices[ i ];
					assert( iDp <= 2 );
				} else if( occurrence[ i ] == 1 ) {
					abc[ iAbc++ ] = uniqueVertices[ i ]; 
					assert( iAbc <= 3 );
				} else {
					assert( false );
				}
			}

			a = abc[ 0 ];
			b = abc[ 1 ];
			c = abc[ 2 ];
			const Vector3<REAL> facePlaneNormal = Vector3<REAL>::normalize( Vector3<REAL>::cross( vertices[ c ] - vertices[ a ], vertices[ b ] - vertices[ a ] ) );
			const REAL facePlaneDist = Vector3<REAL>::dot(facePlaneNormal, vertices[ a ].fromOrigin());
			if ( Vector3<REAL>::dot(facePlaneNormal, vertices[ dp[ 0 ] ].fromOrigin()) - facePlaneDist > -1e-4 ) { // sort it so p falls below ABC and d above
				d = dp[ 0 ];
				p = dp[ 1 ];
			} else {
				d = dp[ 1 ];
				p = dp[ 0 ];
			}
			assert( Vector3<REAL>::dot(facePlaneNormal, vertices[ d ].fromOrigin()) - facePlaneDist > -1e-4 );
			assert( Vector3<REAL>::dot(facePlaneNormal, vertices[ p ].fromOrigin()) - facePlaneDist < 1e-4 );
		}
		
		assert( a >= 0 && b >= 0 && c >= 0 && d >= 0 && p >= 0 );

		// destroy the unused tetrahedron
		Tetrahedron::destroy( tetrahedra[ tetrahedron3 ], tetrahedra );

		// Result 1

		resT1.v[ 0 ] = a;
		resT1.v[ 1 ] = c;
		resT1.v[ 2 ] = b;
		resT1.v[ 3 ] = d;
		Tetrahedron::fixFaceOrientations( resT1, vertices );

		{ // sort out neighbors
			const int sf1 = Tetrahedron::sharedFace( srcT1, resT1, false );
			const int sf2 = Tetrahedron::sharedFace( srcT2, resT1, false );
			const int sf3 = Tetrahedron::sharedFace( srcT3, resT1, false );
			const int sf4 = Tetrahedron::getFaceFromVertices( resT1, a, c, b );
			assert( sf1 >= 0 && sf2 >= 0 && sf3 >= 0 && sf4 >= 0 );

			int v[3];

			srcT1.getFaceVertices( sf1, v[0], v[1], v[2] );
			const int r1sf = Tetrahedron::getFaceFromVertices( resT1, v[0], v[1], v[2] );
			resT1.neighbors[ r1sf ] = srcT1.neighbors[ sf1 ];

			srcT2.getFaceVertices( sf2, v[0], v[1], v[2] );
			const int r2sf = Tetrahedron::getFaceFromVertices( resT1, v[0], v[1], v[2] );
			resT1.neighbors[ r2sf ] = srcT2.neighbors[ sf2 ];

			srcT3.getFaceVertices( sf3, v[0], v[1], v[2] );
			const int r3sf = Tetrahedron::getFaceFromVertices( resT1, v[0], v[1], v[2] );
			resT1.neighbors[ r3sf ] = srcT3.neighbors[ sf3 ];

			adjustNeighborVicinity( iResT1, r1sf, tetrahedra );
			adjustNeighborVicinity( iResT1, r2sf, tetrahedra );
			adjustNeighborVicinity( iResT1, r3sf, tetrahedra );

			resT1.neighbors[ sf4 ] = iResT2;
		}		

		// Result 2

		resT2.v[ 0 ] = a;
		resT2.v[ 1 ] = b;
		resT2.v[ 2 ] = c;
		resT2.v[ 3 ] = p;
		Tetrahedron::fixFaceOrientations( resT2, vertices );

		{ // sort out neighbors
			const int sf1 = Tetrahedron::sharedFace( srcT1, resT2, false );
			const int sf2 = Tetrahedron::sharedFace( srcT2, resT2, false );
			const int sf3 = Tetrahedron::sharedFace( srcT3, resT2, false );
			const int sf4 = Tetrahedron::getFaceFromVertices( resT2, a, b, c );
			assert( sf1 >= 0 && sf2 >= 0 && sf3 >= 0 && sf4 >= 0 );

			int v[3];

			srcT1.getFaceVertices( sf1, v[0], v[1], v[2] );
			const int r1sf = Tetrahedron::getFaceFromVertices( resT2 ,v[0], v[1], v[2] );
			resT2.neighbors[ r1sf ] = srcT1.neighbors[ sf1 ];

			srcT2.getFaceVertices( sf2, v[0], v[1], v[2] );
			const int r2sf = Tetrahedron::getFaceFromVertices( resT2, v[0], v[1], v[2] );
			resT2.neighbors[ r2sf ] = srcT2.neighbors[ sf2 ];

			srcT3.getFaceVertices( sf3, v[0], v[1], v[2] );
			const int r3sf = Tetrahedron::getFaceFromVertices( resT2, v[0], v[1], v[2] );
			resT2.neighbors[ r3sf ] = srcT3.neighbors[ sf3 ];

			adjustNeighborVicinity( iResT2, r1sf, tetrahedra );
			adjustNeighborVicinity( iResT2, r2sf, tetrahedra );
			adjustNeighborVicinity( iResT2, r3sf, tetrahedra );

			resT2.neighbors[ sf4 ] = iResT1;
		}		
		
#if _DEBUG
		for( int i = 0; i < 2; i++ ) {
			assert( Tetrahedron::checkNeighbors( tetrahedra[ resultingTetrahedra[ i ] ], resultingTetrahedra[ i ], tetrahedra, vertices ) );
		}
#endif
	}

	/*
	================
	Delaunay3D::Flip44

	Flips 4 adjacent tetrahedra [abcd][acde][bcdf][cdef] sharing a common edge c-d
	into 4 adjacent tetrahedra [abce][abed][bcef][bdef] sharing a common edge b-e
	================
	*/
	void flip44( const unsigned int tetrahedron1, const unsigned int tetrahedron2, const unsigned int tetrahedron3, const unsigned int tetrahedron4,
				 CoreLib::List< tetrahedron_t >& tetrahedra,
				 const CoreLib::List< Point >& vertices,
				 unsigned int resultingTetrahedra[4] ) {

		
		/* The order of the input tetrahedra is:

					 a                   a
					/|\                 /|\
				   / | \               / | \
				  /  |  \             /  |  \
				 / 1 | 3 \           / 1 | 3 \
			   b/____|d___\e  -->  c/____|e___\d
				\   c|    /         \   b|    /
				 \ 2 | 4 /           \ 2 | 4 /
				  \  |  /             \  |  /
				   \ | /               \ | /
					\|/                 \|/
					 f                   f
		*/

		tetrahedron_t srcT1 = tetrahedra[ tetrahedron1 ];
		tetrahedron_t srcT2 = tetrahedra[ tetrahedron2 ];
		tetrahedron_t srcT3 = tetrahedra[ tetrahedron3 ];
		tetrahedron_t srcT4 = tetrahedra[ tetrahedron4 ];

		assert( Tetrahedron::adjacentTo( srcT1, srcT2 ) );
		assert( Tetrahedron::adjacentTo( srcT1, srcT3 ) );
		assert( Tetrahedron::adjacentTo( srcT2, srcT4 ) );
		assert( Tetrahedron::adjacentTo( srcT3, srcT4 ) );

		assert( Tetrahedron::checkFaceOrientations( srcT1, vertices ) );
		assert( Tetrahedron::checkFaceOrientations( srcT2, vertices ) );
		assert( Tetrahedron::checkFaceOrientations( srcT3, vertices ) );
		assert( Tetrahedron::checkFaceOrientations( srcT4, vertices ) );

		assert( Tetrahedron::checkNeighbors( srcT1, tetrahedron1, tetrahedra, vertices ) );
		assert( Tetrahedron::checkNeighbors( srcT2, tetrahedron2, tetrahedra, vertices ) );
		assert( Tetrahedron::checkNeighbors( srcT3, tetrahedron3, tetrahedra, vertices ) );
		assert( Tetrahedron::checkNeighbors( srcT4, tetrahedron4, tetrahedra, vertices ) );

		const int iResT1 = tetrahedron1; // reuse the source tetrahedron1 slot
		tetrahedron_t& resT1 = tetrahedra[ iResT1 ];
		Tetrahedron::destroy( resT1, tetrahedra );
		const int iResT2 = tetrahedron2; // reuse the source tetrahedron2 slot
		tetrahedron_t& resT2 = tetrahedra[ iResT2 ];
		Tetrahedron::destroy( resT2, tetrahedra );
		const int iResT3 = tetrahedron3; // reuse the source tetrahedron3 slot
		tetrahedron_t& resT3 = tetrahedra[ iResT3 ];
		Tetrahedron::destroy( resT3, tetrahedra );
		const int iResT4 = tetrahedron4; // reuse the source tetrahedron4 slot
		tetrahedron_t& resT4 = tetrahedra[ iResT4 ];
		Tetrahedron::destroy( resT4, tetrahedra );

		assert( !resT1.isValid() );
		assert( !resT2.isValid() );
		assert( !resT3.isValid() );
		assert( !resT4.isValid() );

		resultingTetrahedra[ 0 ] = iResT1;
		resultingTetrahedra[ 1 ] = iResT2;
		resultingTetrahedra[ 2 ] = iResT3;
		resultingTetrahedra[ 3 ] = iResT4;
			
		// extract the vertices by finding the shared faces
		// between each pair of tetrahedra and retrieving
		// the outlying vertices on each side. This way we
		// avoid dealing with the order in which the shared
		// face vertices are declared

		int a, b, c, d, e, f;
		int sf;

		sf = Tetrahedron::sharedFace( srcT1, srcT2, true ); // [bcd]
		assert( sf >= 0 );
		a = Tetrahedron::getVertexOutsideFace( srcT1, sf );
		
		sf = Tetrahedron::sharedFace( srcT2, srcT1, true ); // [bcd]
		assert( sf >= 0 );
		f = Tetrahedron::getVertexOutsideFace( srcT2, sf );

		sf = Tetrahedron::sharedFace( srcT1, srcT3, true ); // [acd]
		assert( sf >= 0 );
		b = Tetrahedron::getVertexOutsideFace( srcT1, sf );

		sf = Tetrahedron::sharedFace( srcT3, srcT1, true ); // [acd]
		assert( sf >= 0 );
		e = Tetrahedron::getVertexOutsideFace( srcT3, sf );

		// The only remaining vertices are c and d
		// Since we know they belong to each one of
		// the source tetrahedra, just identify the 
		// not-yet assigned vertices on any of the
		// source tetrahedra

		// srcT1 [abcd] --> we have a, b already
		c = -1; d = -1;
		for( int i = 0; i < 4 && c < 0 && d < 0; i++ ) {
			if ( Tetrahedron::getVertexOutsideFace( srcT1, i ) == b ) {
				int v[3];
				srcT1.getFaceVertices( i, v[0], v[1], v[2] );
				for( int j = 0; j < 3; j++ ) {
					if ( a == v[ j ] ) {
						c = v[ ( j + 1 ) % 3 ];
						d = v[ ( j + 2 ) % 3 ];
						break;
					}
				}
			}
		} 
		assert( c >= 0 && d >= 0 );

		memcpy( resT1.v, srcT1.v, 4 * sizeof(int) );
		memcpy( resT2.v, srcT2.v, 4 * sizeof(int) );
		memcpy( resT3.v, srcT3.v, 4 * sizeof(int) );
		memcpy( resT4.v, srcT4.v, 4 * sizeof(int) );
		for( int i = 0; i < 4; i++ ) {
			if ( resT1.v[ i ] == d ) resT1.v[ i ] = e;
			if ( resT2.v[ i ] == d ) resT2.v[ i ] = e;
			if ( resT3.v[ i ] == c ) resT3.v[ i ] = b;
			if ( resT4.v[ i ] == c ) resT4.v[ i ] = b;
		}

		// Result 1
		
		{ // sort out neighbors

			// get the external faces in the new tetrahedron...
			const int sf1 = Tetrahedron::getFaceFromVertices( srcT1, a, b, c );
			const int sf3 = Tetrahedron::getFaceFromVertices( srcT3, a, c, e );
			const int rf1 = Tetrahedron::getFaceFromVertices( resT1, a, b, c );
			const int rf3 = Tetrahedron::getFaceFromVertices( resT1, a, c, e );
			assert( sf1 >= 0 && sf3 >= 0 && rf1 >= 0 && rf3 >= 0 );

			resT1.neighbors[ rf1 ] = srcT1.neighbors[ sf1 ];
			resT1.neighbors[ rf3 ] = srcT3.neighbors[ sf3 ];

			adjustNeighborVicinity( iResT1, rf1, tetrahedra );
			adjustNeighborVicinity( iResT1, rf3, tetrahedra );
		}

		// Result 2

		{ // sort out neighbors
			const int sf2 = Tetrahedron::getFaceFromVertices( srcT2, b, c, f );
			const int sf4 = Tetrahedron::getFaceFromVertices( srcT4, c, e, f );
			const int rf2 = Tetrahedron::getFaceFromVertices( resT2, b, c, f );
			const int rf4 = Tetrahedron::getFaceFromVertices( resT2, c, e, f );
			assert( sf2 >= 0 && sf4 >= 0 && rf2 >= 0 && rf4 >= 0 );

			resT2.neighbors[ rf2 ] = srcT2.neighbors[ sf2 ];
			resT2.neighbors[ rf4 ] = srcT4.neighbors[ sf4 ];

			adjustNeighborVicinity( iResT2, rf2, tetrahedra );
			adjustNeighborVicinity( iResT2, rf4, tetrahedra );
		}

		// Result 3


		{ // sort out neighbors
			const int sf1 = Tetrahedron::getFaceFromVertices( srcT1, a, b, d );
			const int sf3 = Tetrahedron::getFaceFromVertices( srcT3, a, d, e );
			const int rf1 = Tetrahedron::getFaceFromVertices( resT3, a, b, d );
			const int rf3 = Tetrahedron::getFaceFromVertices( resT3, a, d, e );
			assert( sf1 >= 0 && sf3 >= 0 && rf1 >= 0 && rf3 >= 0 );

			resT3.neighbors[ rf1 ] = srcT1.neighbors[ sf1 ];
			resT3.neighbors[ rf3 ] = srcT3.neighbors[ sf3 ];

			adjustNeighborVicinity( iResT3, rf1, tetrahedra );
			adjustNeighborVicinity( iResT3, rf3, tetrahedra );
		}

		// Result 4


		{ // sort out neighbors
			
			const int sf2 = Tetrahedron::getFaceFromVertices( srcT2, b, d, f );
			const int sf4 = Tetrahedron::getFaceFromVertices( srcT4, d, e, f );
			const int rf2 = Tetrahedron::getFaceFromVertices( resT4, b, d, f );
			const int rf4 = Tetrahedron::getFaceFromVertices( resT4, d, e, f );

			assert( sf2 >= 0 && sf4 >= 0 && rf2 >= 0 && rf4 >= 0 );

			resT4.neighbors[ rf2 ] = srcT2.neighbors[ sf2 ];
			resT4.neighbors[ rf4 ] = srcT4.neighbors[ sf4 ];

			adjustNeighborVicinity( iResT4, rf2, tetrahedra );
			adjustNeighborVicinity( iResT4, rf4, tetrahedra );
		}

		{
			// Fix adjacency between resulting tetrahedra
			resT1.neighbors[ Tetrahedron::getFaceFromVertices( resT1, b, c, e ) ] = iResT2;
			resT1.neighbors[ Tetrahedron::getFaceFromVertices( resT1, a, b, e ) ] = iResT3;
			resT2.neighbors[ Tetrahedron::getFaceFromVertices( resT2, b, c, e ) ] = iResT1;
			resT2.neighbors[ Tetrahedron::getFaceFromVertices( resT2, b, e, f ) ] = iResT4;
			resT3.neighbors[ Tetrahedron::getFaceFromVertices( resT3, a, b, e ) ] = iResT1;
			resT3.neighbors[ Tetrahedron::getFaceFromVertices( resT3, b, d, e ) ] = iResT4;
			resT4.neighbors[ Tetrahedron::getFaceFromVertices( resT4, b, d, e ) ] = iResT3;
			resT4.neighbors[ Tetrahedron::getFaceFromVertices( resT4, b, e, f ) ] = iResT2;
		}

#if _DEBUG
		for( int i = 0; i < 4; i++ ) {
			assert( Tetrahedron::checkNeighbors( tetrahedra[ resultingTetrahedra[ i ] ], resultingTetrahedra[ i ], tetrahedra, vertices ) );	
		}
#endif
		Tetrahedron::fixFaceOrientations( resT1, vertices );
		Tetrahedron::fixFaceOrientations( resT2, vertices );
		Tetrahedron::fixFaceOrientations( resT3, vertices );
		Tetrahedron::fixFaceOrientations( resT4, vertices );
	}

	//////////////////////////////////////////////////////////////////////////
	// Auxiliary functions
	//////////////////////////////////////////////////////////////////////////

	/*
	================
	Delaunay3D::Containingtetrahedron

	Generates a big tetrahedron t containing the bounding sphere
	The vertices of t do *not* belong to the provided point set,
	and therefore are returned as < 0 indices referencing the
	temp vertices array obtained by ::TempVertices();
	================
	*/
	void containingTetrahedron( const RenderLib::Math::Point3f& center, const float radius, // circumsphere
								tetrahedron_t& t,
								CoreLib::List< Point >& pointSet ) {

		// from the Wikipedia: 
		// For a regular tetrahedron of edge length L: radius = L / sqrt( 24 )
		// http://en.wikipedia.org/wiki/Tetrahedron#Formulas_for_regular_tetrahedron

		const REAL sqRoot = sqrt( 24.0 );
		const REAL L = radius * sqRoot;

		/*
					L/2       L/2
				O---------+----------      regular triangle, A = 60 deg
				\ A       |       A /      h = h1 + h2 = L * sin(A)
				 \        |h1      /       angle (O+),(OP) = A/2 = 30 deg
				  \       P       /        tan(A/2) = h1 / (L/2)
				   \      |      /         h1 = tan(30) * L / 2
				 L  \     |     / L		   h2 = h - h1 = L * sin(60) - h1
					 \    |h2  /
					  \   |   /
					   \  |  /
						\ A /
						 \|/

		*/

		const REAL h1 = tan( DEG2RAD(30.0) ) * L / 2;
		const REAL h2 = sin( DEG2RAD(60.0) ) *  L - h1 ;

		/*
			y
			|
			|
			+----> x
		   /
		  z		p1 ------------_- p2
				  \     ___--- /        p1 = ( P.x - L/2,	P.y + radius,		P.z - h1 )
				   \_---      /         p2 = ( P.x + L/2,	P.y + radius,		P.z - h1 )
				  p3\    P   /          p3 = ( P.x,			P.y + radius,		P.z + h2 )
					 \      /           p4 = ( P.x,			P.y + radius - h,	P.z )
					  \    /
					   \  /
						p4
		*/ 

		const REAL margin = L * (REAL)0.005;

		const Point p1( (float)( center.x - L / 2 - margin ), (float)( center.y + radius + margin ), (float)( center.z - h1 - margin ) );
		const Point p2( (float)( center.x + L / 2 + margin ), (float)( center.y + radius + margin ), (float)( center.z - h1 - margin ) );
		const Point p3( (float)( center.x ), (float)( center.y + radius + margin ), (float)( center.z + h2 + margin ) );
		const Point p4( (float)( center.x ), (float)( center.y + radius - h1 - h2 + margin ), (float)( center.z ) );

		t.v[ 0 ] = pointSet.append( p1 );
		t.v[ 1 ] = pointSet.append( p2 );
		t.v[ 2 ] = pointSet.append( p3 );
		t.v[ 3 ] = pointSet.append( p4 );
	}
	/*
	================
	Delaunay3D::AdjustNeighborVicinity

	Given a tetrahedron and a face, ensures the potential adjacent tetrahedron sharing
	that face points to the given tetrahedron.
	================
	*/
	void adjustNeighborVicinity( const int iT, const int f, CoreLib::List< tetrahedron_t >& tetrahedra ) {
		const tetrahedron_t& T = tetrahedra[ iT ];
		assert( T.isValid() && "IsValid(T)" );
		if( T.neighbors[ f ] >= 0 ) { // adjust our neighbor's vicinity
			tetrahedron_t& n = tetrahedra[ T.neighbors[ f ] ];
			if ( n.isValid() ) {
				int v[3];
				T.getFaceVertices( f, v[0], v[1], v[2] );
				const int sharedFace = Tetrahedron::getFaceFromVertices( n, v[0], v[2], v[1] ); // reverse face
				assert( sharedFace >= 0 && "sharedFace >= 0" );
				n.neighbors[ sharedFace ] = iT;
			}
		}
	}

	/*
	================
	Delaunay3D::Walk

	Returns the index of the tetrahedron containing p, or -1 if not found. 
	It retrieves the result by searching from a given sourceT tetrahedron index. 
	================
	*/
	int walk( const Point& p, const int sourceT, 
			  const CoreLib::List< Point >& vertices,
			  const CoreLib::List< tetrahedron_t >& tetrahedra ) {
		if ( sourceT < 0 || (size_t)sourceT >= tetrahedra.size() ) {
			assert( false );
			return -1;
		}

		CoreLib::List< bool > visited;
		visited.resize( tetrahedra.size(), true );
		for( size_t i = 0; i < tetrahedra.size(); i++ ) {
			visited[ (unsigned int)i ] = false;
		}
		CoreLib::List< int > traversal( 64 );
		
		int t = sourceT;

		while( true ) {
			
			const tetrahedron_t& tetrahedron = tetrahedra[ t ];
			visited[ t ] = true;
			traversal.append( t );
			bool step = false;

			if ( tetrahedron.isValid() ) {

				if ( inside( p, tetrahedron, vertices ) ) {
					return t;
				}

				// pick adjacent neighbor such as p lies on the positive side
				// of their shared face
				int a, b, c;
				for( int i = 0; i < 4; i++ ) {
					if ( tetrahedron.neighbors[ i ] >= 0 ) {
						if ( visited[ tetrahedron.neighbors[ i ] ] ) {
							continue;
						}
						tetrahedron.getFaceVertices( i, a, b, c );
						if ( orient( vertices[ a ], vertices[ b ], vertices[ c ], p ) > 0 ) {
							t = tetrahedron.neighbors[ i ];
							step = true;
							break;
						}
					}
				}
			}

			if ( !step ) {
				// we reached a dead end. Retry from another source tetrahedron

				bool found = false;
				for( size_t i = 0; i < visited.size(); i++ ) {
					if ( !visited[ (unsigned int)i ] ) {
						t = (int)i;
						found = true;
						break;
					}
				}

				traversal.resize( 0, false );

				if ( !found ) {
					assert( false );
					return -1;
				}
			}			
		}
		
		assert( false );
		return -1;
	}

	/*
	================
	Delaunay3D::Flip

	Flips two given tetrahedra: T and Ta, which are non-Delaunay, 
	into a Delaunay configuration using bistellar flips. The
	resulting tetrahedra's adjacents will need to be tested for
	Delaunay-ness, and they're inserted into the needTesting stack
	================
	*/
	
	void flip( const int iT, const int iTa, const int p,
			   const CoreLib::List< Point >& vertices, 
			   CoreLib::List< tetrahedron_t >& tetrahedra,
			   std::stack< unsigned int >& needTesting ) {
		
		using namespace RenderLib::Geometry;

		tetrahedron_t& T = tetrahedra[ iT ];
		tetrahedron_t& Ta = tetrahedra[ iTa ];

		assert( Tetrahedron::adjacentTo( T, Ta ) );
		assert( T.containsVertex( p ) );

		const int TSharedFace = Tetrahedron::sharedFace( T, Ta, true );
		assert( TSharedFace >= 0 );

		int sharedVertices[3];
		T.getFaceVertices( TSharedFace, sharedVertices[ 0 ], sharedVertices[ 1 ], sharedVertices[ 2 ] );

		const int d = Tetrahedron::getVertexOutsideFace( Ta, Tetrahedron::sharedFace( Ta, T, true ) );
		assert( d >= 0 );

		int Case = -1;

		const Point& pA = vertices[ sharedVertices[ 0 ] ];
		const Point& pB = vertices[ sharedVertices[ 1 ] ];
		const Point& pC = vertices[ sharedVertices[ 2 ] ];
		const Point& pP = vertices[ p ];
		const Point& pD = vertices[ d ];

		if( coplanar( pA, pB, pC, pP ) ) {
			// case 4 = a, b, c, p are coplanar. T is flat.
			Case = 4;
			assert( Tetrahedron::isFlat( T, vertices ) );
		} else if ( coplanar( pA, pB, pD, pP ) ) {
			Case = 31;
		} else if ( coplanar( pA, pC, pD, pP ) ) {
			Case = 32;
		} else if ( coplanar( pB, pC, pD, pP ) ) {
			Case = 33;
		} else {
			REAL t,v,w;
			if ( segmentTriangleIntersect_DoubleSided( pP, pD, pA, pB, pC, t, v, w ) ) {
				// the segment p-d crosses the shared face. Therefore the union between T and Ta
				// is convex
				Case = 1;
			} else {
				// the segment p-d does not cross the shared face
				Case = 2;
			}
		}

#if _DEBUG
		if ( Case != 4 ) {
			assert( !Tetrahedron::isFlat( T, vertices ) );
		}
#endif

		switch( Case ) {
			case 1:
				{
					// Case 1: only one face of Ta is visible from the non-shared 
					// vertex of T, and therefore the union of  and a is a convex 
					// polyhedron. In this case, a flip23 is performed.

					unsigned int result[3];
					if ( flip23( iT, iTa, tetrahedra, vertices, result ) ) {
						for( int i = 0; i < 3; i++ ) {
							needTesting.push( result[ i ] );
						} 
					} 
				}
				break;
			case 4:
				{			
					// According to the paper:

					// Case 4: Degenerate case: the tetrahedron abcp is flat
					// the vertex p is directly on one edge of the
					// face abc (assume it is ab here). Thus, p lies in the
					// same plane as abc, but also as abd. The tetrahedron
					// abcp is obviously flat. The only reason for
					// such a case is because p was inserted directly on
					// one edge of T in the first place (the edge ab), and
					// the first flip14 to split the tetrahedron containing
					// p created two flat tetrahedra. Because p was
					// inserted on the edge ab, all the tetrahedra in T
					// incident to ab must be split into two tetrahedra.
					// This could be done with the degenerate flip12, but
					// it can also be done simply with a sequence of flip23
					// and flip32. When the case #4 happens, it suffices
					// to perform a flip23 on T and Ta. Obviously, another
					// flat tetrahedron abdp will be created, but
					// this one will deleted by another flip.

					// However we can't apply a flip23 blindly! Since this is an edge case
					// unlike when we're in Case 1, we can't be sure there won't be yet
					// another (flat) tetrahedron sharing the faces of the tetrahedra we're
					// intending to swap (in this case one of the 3 resulting tetrahedra
					// would completely overlap the external one, and this is invalid). 
					// The idea is performing a fli23 -> flip32 sequence according to the paper
					// so we'll first check for the existence of a tetrahedron linking our
					// 2 candidates, which would give us the config for the eventual flip32 directly
					// such as the Case #2:

					// In short:
					//		Got two tetrahedra T and Ta in Case 4 (T is flat)
					//			* if there exists Tb adjacent to both T and Ta --> perform flip32 (like Case 2)
					//			* otherwise --> perform flip23 (like Case 1) resulting in 
					//			another flat tetrahedron that we'll remove later on

					bool fixed = false;
					for( int s = 0; !fixed && s < 3; s++ ) {
						const int a = sharedVertices[ s ];
						const int b = sharedVertices[ ( s + 1 ) % 3 ];
						const int abp = Tetrahedron::getFaceFromVertices( T, a, b, p );
						const int bap = Tetrahedron::getFaceFromVertices( Ta, b, a, d );
#if _DEBUG
						if ( abp < 0 ) {
							assert( Tetrahedron::getFaceFromVertices( T, b, a, p ) < 0 );
							assert( Tetrahedron::getFaceFromVertices( T, b, p, a ) < 0 );
						}
						if ( bap < 0 ) {
							assert( Tetrahedron::getFaceFromVertices( Ta, a, b, p ) < 0 );
							assert( Tetrahedron::getFaceFromVertices( Ta ,a, p, b ) < 0 );
						}
#endif
						if ( abp >= 0 && bap >= 0 ) {
							const int iTb1 = T.neighbors[ abp ];
							const int iTb2 = Ta.neighbors[ bap ];
							if ( iTb1 >= 0 && iTb1 == iTb2 ) {
#if _DEBUG
								tetrahedron_t& Tb = tetrahedra[ iTb1 ];								
								assert( Tetrahedron::getVertexOutsideFace( Tb, Tetrahedron::getFaceFromVertices( Tb, a, b, p ) ) == d );
#endif
								fixed = true;
								unsigned int result[2];
								flip32( iT, iTa, iTb1, tetrahedra, vertices, result );
								needTesting.push( result[ 0 ] );
								needTesting.push( result[ 1 ] );
							}
						}
					}

					if ( !fixed ) {
						unsigned int result[3];
						if ( flip23( iT, iTa, tetrahedra, vertices, result ) ) {
							for( int i = 0; i < 3; i++ ) {
								needTesting.push( result[ i ] );
							} 
						} else {
							// flip not possible
							if ( Case == 4 ) {
								// T was flat and it was not possible to get rid of it. Erase. 
								//T.Destroy( tetrahedra );
							}
						} 
					}
				}
				break;
			case 2:
				{
					// Case 2: two faces of Ta are visible, and therefore
					// the union of T and Ta is non-convex. If there exists
					// a tetrahedron Tb = abpd such that the
					// edge ab is shared by  T, Ta and Tb, then a flip32 can
					// be performed. If there exists no such tetrahedron,
					// then no flip is performed. The non-locally Delaunay
					// facet abc will be ‘rectified’ by another flip
					// performed on adjacent tetrahedra.

					bool fixed = false;
					for( int s = 0; !fixed && s < 3; s++ ) {
						const int a = sharedVertices[ s ];
						const int b = sharedVertices[ ( s + 1 ) % 3 ];
						const int abp = Tetrahedron::getFaceFromVertices( T, a, b, p );
						const int bap = Tetrahedron::getFaceFromVertices( Ta, b, a, d );
#if _DEBUG
						if ( abp < 0 ) {
							assert( Tetrahedron::getFaceFromVertices( T ,b, a, p ) < 0 );
							assert( Tetrahedron::getFaceFromVertices( T, b, p, a ) < 0 );
						}
						if ( bap < 0 ) {
							assert( Tetrahedron::getFaceFromVertices( Ta, a, b, p ) < 0 );
							assert( Tetrahedron::getFaceFromVertices( Ta, a, p, b ) < 0 );
						}
#endif
						if ( abp >= 0 && bap >= 0 ) {
							const int iTb1 = T.neighbors[ abp ];
							const int iTb2 = Ta.neighbors[ bap ];
							if ( iTb1 >= 0 && iTb1 == iTb2 ) {
#if _DEBUG
								tetrahedron_t& Tb = tetrahedra[ iTb1 ];								
								assert( Tetrahedron::getVertexOutsideFace( Tb, Tetrahedron::getFaceFromVertices( Tb, a, b, p ) ) == d );
#endif
								fixed = true;
								unsigned int result[2];
								flip32( iT, iTa, iTb1, tetrahedra, vertices, result );
								needTesting.push( result[ 0 ] );
								needTesting.push( result[ 1 ] );
								assert( tetrahedra[ result[ 0 ] ].containsVertex( a ) || tetrahedra[ result[ 1 ] ].containsVertex( a ) );
								assert( tetrahedra[ result[ 0 ] ].containsVertex( b ) || tetrahedra[ result[ 1 ] ].containsVertex( b ) );
								assert( tetrahedra[ result[ 0 ] ].containsVertex( p ) );
								assert( tetrahedra[ result[ 1 ] ].containsVertex( p ) );
								assert( tetrahedra[ result[ 0 ] ].containsVertex( d ) );
								assert( tetrahedra[ result[ 1 ] ].containsVertex( d ) );
#if _DEBUG
								const int c = sharedVertices[ ( s + 2 ) % 3 ];
								assert( tetrahedra[ result[ 0 ] ].containsVertex( c ) );
								assert( tetrahedra[ result[ 1 ] ].containsVertex( c ) );
#endif

								assert( !tetrahedra[ iTb1 ].isValid() );
							}
						}
					}
				}
				break;

			case 31:
			case 32:
			case 33:
				{
					// Degenerate case: p,a,b,d are coplanar

					// the line segment pd intersects directly one
					// edge of abc (assume it is ab here). Thus, vertices p,
					// d, a and b are coplanar. Observe that a flip23, that
					// would create a flat tetrahedron abdp, is possible if
					// and only if T and Ta are in config44
					// with two other tetrahedra Tb and Tc, such that the
					// edge ab is incident to T, Ta, Tb and Tc. Since the
					// four tetrahedra are in config44, a flip44 can be
					// performed. If not, no flip is performed.

					int sharedSegmentA, sharedSegmentB;
					if ( Case == 31 ) {
						// ab
						sharedSegmentA = sharedVertices[ 0 ];
						sharedSegmentB = sharedVertices[ 1 ];
					} else if ( Case == 32 ) {
						// ac
						sharedSegmentA = sharedVertices[ 0 ];
						sharedSegmentB = sharedVertices[ 2 ];
					} else {
						// bc
						sharedSegmentA = sharedVertices[ 1 ];
						sharedSegmentB = sharedVertices[ 2 ];
					}
					assert( coplanar( vertices[ sharedSegmentA ], vertices[ sharedSegmentB ], vertices[ p ], vertices[ d ] ) );
					int faceT = Tetrahedron::getFaceFromVertices( T, sharedSegmentA, sharedSegmentB, p );				
					int faceTa = Tetrahedron::getFaceFromVertices( Ta, sharedSegmentB, sharedSegmentA, d );
					assert( faceT >= 0 && faceTa >= 0 );

					int iNeighborT = T.neighbors[ faceT ];
					int iNeighborTa = Ta.neighbors[ faceTa ];
					if ( iNeighborT >= 0 && iNeighborTa >= 0 ) {
						tetrahedron_t& Tb = tetrahedra[ iNeighborT ];
						tetrahedron_t& Tc = tetrahedra[ iNeighborTa ];
						if ( Tb.isValid() && 
							 Tc.isValid() && 
							 Tetrahedron::adjacentTo( Tb, Tc ) ) {


							int sharedFaceT_Tb = Tetrahedron::getFaceFromVertices( Tb, sharedSegmentA, sharedSegmentB, p );
							assert( sharedFaceT_Tb >= 0 );							
							const int c = Tetrahedron::getVertexOutsideFace( T, faceT );
							assert( Tetrahedron::getVertexOutsideFace( Ta, faceTa ) == c );
							const int d = Tetrahedron::getVertexOutsideFace( Tb, sharedFaceT_Tb );
#if _DEBUG
							{
								int ta0, ta1, ta2;
								Ta.getFaceVertices( faceTa, ta0, ta1, ta2 );
								assert( Tetrahedron::getVertexOutsideFace( Tc, Tetrahedron::getFaceFromVertices( Tc ,ta0, ta2, ta1 ) ) == d );
							}
#endif
							// Flip44 can be symmetrical around the edge sharedSegmentA <-> sharedSegmentB, so the flat plane
							// can be [ ssA, ssB, c, d ] or it's 90deg rotation [ ssA, ssB, pD, pP ]
							if ( coplanar( vertices[ sharedSegmentA ], vertices[ sharedSegmentB ], vertices[ c ], vertices[ d ] ) ) {
								unsigned int result[4];
								flip44( iT, iTa, iNeighborT, iNeighborTa, tetrahedra, vertices, result );
								for( int i = 0; i < 4; i++ ) {
									needTesting.push( result[ i ] );
								}
							} else if ( coplanar( vertices[ sharedSegmentA ], vertices[ sharedSegmentB ], pD, pP ) ) {
								unsigned int result[4];
								flip44( iT, iNeighborT, iTa, iNeighborTa, tetrahedra, vertices, result );
								for( int i = 0; i < 4; i++ ) {
									needTesting.push( result[ i ] );
								}
							}
						}
					}
				}
				break;

			default:
				// something went wrong here
				assert( false );
				return;
		}
	}

	/*
	================
	Delaunay3D::InsertOnePoint
	================
	*/
	void insertOnePoint( const CoreLib::List< Point >& vertices, const int pointIndex, 
						 CoreLib::List< tetrahedron_t >& tetrahedra ) {
		
		// Find the tetrahedron containing pointIndex
		int t = walk( vertices[ pointIndex ], 0, vertices, tetrahedra );
		if ( t < 0 ) {
			assert( false );
			return;
		}
		
		// Insert pointIndex into t using a flip14
		unsigned int result[4];
		flip14( pointIndex, t, tetrahedra, vertices, result );

		std::stack< unsigned int > stack;
		for( int i = 0; i < 4; i++ ) {
			stack.push( result[ i ] );
		}

		while( !stack.empty() ) {
#if _DEBUG && 0
			{
				for( int i  = 0; i < (int)stack.size() - 1; i++ ) {
					for( unsigned int j = i + 1 ; j < stack.size(); j++ ) {
						assert( stack._Get_container()[ i ] != stack._Get_container()[ j ] );		
					}
				}
			}
#endif
			// T = { a, b, c, p }
			const int iT = stack.top();
			stack.pop();
			tetrahedron_t& T = tetrahedra[ iT ];
			if ( !T.isValid() ) { // might have been invalidated by another flip
				continue;
			}
					
			// T = { a, b, c, p }
			int face = 0;
			for( ; face < 4; face++ ) {
				if ( Tetrahedron::getVertexOutsideFace( T, face ) == pointIndex ) {
					break;
				}
			}
			if ( face == 4 ) {
				continue; // something went wrong. skip the tetrahedron
			}

			assert( Tetrahedron::getVertexOutsideFace( T, face ) == pointIndex );

			// Get adjacent tetrahedron Ta having a,b,c as a facet
			if ( T.neighbors[ face ] >= 0 ) {
				const int iTa = T.neighbors[ face ];
				tetrahedron_t& Ta = tetrahedra[ iTa ];			
				assert( Ta.isValid() );

				int a,b,c;
				T.getFaceVertices( face, a, b, c );
				const int sharedFace = Tetrahedron::getFaceFromVertices( Ta, a, c, b ); // reverse face
				const int opposedVertex = Tetrahedron::getVertexOutsideFace( Ta, sharedFace );

				// OpposedVertex CAN be pointIndex in a degenerate Case 1, where two adjacent tetrahedra also share
				// the vertex opposed to their shared face (that is, they're effectively the same tetrahedron with the 
				// faces declared in a different order). This can happen after inserting a vertex in the face of an existing
				// tetrahedron and causing N chained Case 4 till we close the cycle and reach the original point again.
				// This degenerate Case1 will generate 3 new (smaller) flat tetrahedra, that should eventually go away
				// with further flips
				// ! don't --> assert( opposedVertex != pointIndex );

				const Point& d = vertices[ opposedVertex ];
				
				// If d is inside the circumference of T then flip 
				const Point& T0 = vertices[ T.v[ 0 ] ];
				const Point& T1 = vertices[ T.v[ 2 ] ];
				const Point& T2 = vertices[ T.v[ 1 ] ];
				const Point& T3 = vertices[ T.v[ 3 ] ];							

				const bool doFlip =	Tetrahedron::isFlat( T, vertices ) || // if our tetrahedron is flat, it is not valid and we must flip it
									( orient( T0, T1, T2, T3 ) >= 0 && inSphere( T0, T1, T2, T3, d ) > 0 ) || 
									( orient( T0, T2, T1, T3 ) >= 0 && inSphere( T0, T2, T1, T3, d ) > 0 );				
				if ( doFlip ) {
					flip( iT, iTa, pointIndex, vertices, tetrahedra, stack );
				}
			}
		}
	}

	int intCompare( const void* p0, const void* p1 ) {
		return *(static_cast< const int* >(p0)) - *(static_cast< const int* >(p1));  
	}


} // namespace internal


//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
// tetrahedron_t public interface
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
tetrahedron_t::tetrahedron_t()
{
	internal::Tetrahedron::markInvalid(*this);
}

tetrahedron_t::tetrahedron_t( const tetrahedron_t& other )
{
	memcpy( v, other.v, 4 * sizeof( int ) );
	memcpy( neighbors, other.neighbors, 4 * sizeof( int ) );
	memcpy( face, other.face, 3 * 4 * sizeof( int ) );
}

bool tetrahedron_t::isValid() const {
	for( int i = 0; i < 4; i++ ) {
		if ( v[ i ] < 0 ) {
			return false;
		}
	}
	return true;
}

bool tetrahedron_t::containsVertex( const int vert ) const {
	return v[ 0 ] == vert || v[ 1 ] == vert || v[ 2 ] == vert || v[ 3 ] == vert;
}

void tetrahedron_t::getFaceVertices( const int f, int& a, int& b, int& c ) const {
	assert( f >= 0 && f < 4 );
	assert( face[f][0] >= 0 && face[f][0] < 4 );
	assert( face[f][1] >= 0 && face[f][1] < 4 );
	assert( face[f][2] >= 0 && face[f][2] < 4 );
	a = v[ face[f][0] ]; 
	b = v[ face[f][1] ]; 
	c = v[ face[f][2] ];
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
// Delaunay3D public interface
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if LEAVE_CONTAINING_TETRAHEDRON 
bool Delaunay3D::tetrahedralize( CoreLib::List< Point >& srcPoints,
								CoreLib::List< tetrahedron_t >& tetrahedra ) {
#else
bool Delaunay3D::tetrahedralize( const CoreLib::List< Point >& srcPoints,
								CoreLib::List< tetrahedron_t >& tetrahedra ) {
#endif
									using namespace RenderLib::Math;
									using namespace RenderLib::Geometry;

									if ( srcPoints.size() == 0 ) { 
										return false;
									}

									BoundingBox bounds;
									for( size_t i = 0; i < srcPoints.size(); i++ ) {
										bounds.expand( srcPoints[ (unsigned int)i ] );
									}

									Point3f center;
									float radius;
									bounds.boundingSphere( center, radius );
									radius *= 2; // avoid a too tight bound since we want the containing tetrahedron's faces to wrap all the points
									if ( srcPoints.size() < 2 ) {
										// we don't have enough points to define a volume
										// so give it a fixed extra radius
										radius = 1.0;
									}

									// copy the source points list, we will need to add some extra
									// points while building the tessellation, but then we'll restrict
									// the resulting tetrahedra to the initial point set.
#if	LEAVE_CONTAINING_TETRAHEDRON
									CoreLib::List< Point >& pointSet = srcPoints;
#else
									CoreLib::List< Point > pointSet = srcPoints;
#endif
									const size_t numSrcPoints = srcPoints.size();

									// generate the tetrahedron from the sphere it contains (circumsphere)
									tetrahedron_t& bigT = tetrahedra.append();
									internal::containingTetrahedron( center, radius, bigT, pointSet );
									internal::Tetrahedron::fixFaceOrientations( bigT, pointSet );

									tetrahedra.setGranularity( 4 * numSrcPoints );
									for( size_t i = 0; i < numSrcPoints; i++ ) {
										internal::insertOnePoint( pointSet, (int)i, tetrahedra );
									}
#if _DEBUG
									// verify Delaunay condition (empty spheres) for all tetrahedra
									for( size_t i = 0; i < tetrahedra.size(); i++ ) {
										const tetrahedron_t& T = tetrahedra[ (unsigned int)i ];
										if ( !T.isValid() ) {
											continue;
										}
										const Point& T0 = pointSet[ T.v[ 0 ] ];
										const Point& T1 = pointSet[ T.v[ 2 ] ];
										const Point& T2 = pointSet[ T.v[ 1 ] ];
										const Point& T3 = pointSet[ T.v[ 3 ] ];
										for( size_t j = 0; j < pointSet.size(); j++ ) {
											assert( internal::inSphere( T0, T1, T2, T3, pointSet[ (unsigned int)j ] ) <= 0 );
										}
									}
#endif
#if !LEAVE_CONTAINING_TETRAHEDRON
									// Delete all tetrahedra containing one of the additional vertices
									// inserted for the containing tetrahedron
									for( size_t i = 0; i < tetrahedra.size(); i++ ) {
										tetrahedron_t& t = tetrahedra[ i ];
										for( size_t j = 0; j < 4; j++ ) {
											if( t.v[ j ] >= srcPoints.size() ) {
												Tetrahedron::destroy( t, tetrahedra );
												break;
											}
										}			
									}
#endif
									return true;
}


} // namespace Delaunay3D
} // namespace Geometry
} // namespace RenderLib
