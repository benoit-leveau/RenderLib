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
#include "math/algebra/point/point3.h"
#include "math/algebra/vector/vector3.h"

namespace RenderLib {
namespace Geometry {
template<typename T>
inline bool SegmentTriangleIntersect_SingleSided( const RenderLib::Math::Point3<T>& p, const RenderLib::Math::Point3<T>& q, 
												  const RenderLib::Math::Point3<T>& a, const RenderLib::Math::Point3<T>& b, const RenderLib::Math::Point3<T>& c, 
												  float& t, float& v, float& w ) {
	using namespace RenderLib::Math;

	Vector3<T> ab = b - a;
	Vector3<T> ac = c - a;
	Vector3<T> qp = p - q;
	Vector3<T> n = Vector3<T>::cross( ab, ac );

	float d = Vector3<T>::dot( qp, n );
	if ( d <= 0.0f ) {
		return false;
	}

	Vector3<T> ap = p - a;
	t = Vector3<T>::dot( ap, n );
	if (t < 0.0f || t > d) {
		return false;
	}

	Vector3<T> e = Vector3<T>::cross(qp, ap);
	v = Vector3<T>::dot(ac, e);
	if (v < 0.0f || v > d) {
		return false;
	}

	w = -Vector3<T>::dot(ab, e);
	if (w < 0.0f || v + w > d) {
		return false;
	}

	float ood = 1.0f / d;
	t *= ood;
	v *= ood;
	w *= ood;

	return true;
}

template< typename T >
inline bool SegmentTriangleIntersect_DoubleSided( const RenderLib::Math::Point3<T>& p, const RenderLib::Math::Vector3<T>& dir, 
												 T minT, T maxT, 
												 const RenderLib::Math::Point3<T>& a, const RenderLib::Math::Point3<T>& b, const RenderLib::Math::Point3<T>& c, 
												 T& t, T& v, T& w,
												 const T epsilon = 1e-5 ) {
	using namespace RenderLib::Math;

	Vector3<T> edge1 = b - a;
	Vector3<T> edge2 = c - a;

	Vector3<T> pvec = Vector3<T>::cross(dir, edge2);
	T det = Vector3<T>::dot(edge1, pvec);
	if ( det > -(T)epsilon && det < (T)epsilon ) {
	return 0;
	}
	T inv_det = 1.0f / det;
	Vector3<T> tvec = p - a;

	v = Vector3<T>::dot(tvec, pvec) * inv_det;
	if ( v < 0 || v > 1 ) {
	return 0;
	}

	Vector3<T> qvec = Vector3<T>::cross(tvec, edge1);
	w = Vector3<T>::dot(dir, pvec) * inv_det;
	if ( w < 0 || v + w > 1 ) {
	return 0;
	}

	t = Vector3<T>::dot(edge2, qvec) * inv_det;

	return ( t >= minT && t <= maxT );
}

template< typename T >
inline bool SegmentTriangleIntersect_DoubleSided( const RenderLib::Math::Point3<T>& p, const RenderLib::Math::Point3<T>& q, 
												 const RenderLib::Math::Point3<T>& a, const RenderLib::Math::Point3<T>& b, const RenderLib::Math::Point3<T>& c, 
												 T& t, T& v, T& w,
												 const T epsilon = 1e-5 ) {		
	using namespace RenderLib::Math;
	Vector3<T> dir = q - p;
	T maxT = dir.Normalize();
	const T lengthEpsilon = 1e-4;
	return SegmentTriangleIntersect_DoubleSided<T>( p, dir, -lengthEpsilon, maxT + lengthEpsilon, a, b, c, t, v, w, epsilon );
}

} // namespace Geometry
} // namespace RenderLib