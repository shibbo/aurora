/* Copyright 1998 - 2006 Nintendo. All rights reserved. */

#include <assert.h>
#include <math.h>
#include <revolution/mtx/mtx44ext.h>

void C_MTX44Identity( Mtx44 m )
{

	assert((m != 0));

	m[0][0] = 1.0f; m[0][1] = 0.0f; m[0][2] = 0.0f; m[0][3] = 0.0f;

	m[1][0] = 0.0f; m[1][1] = 1.0f; m[1][2] = 0.0f; m[1][3] = 0.0f;

	m[2][0] = 0.0f; m[2][1] = 0.0f; m[2][2] = 1.0f; m[2][3] = 0.0f;

	m[3][0] = 0.0f; m[3][1] = 0.0f; m[3][2] = 0.0f; m[3][3] = 1.0f;

}

void C_MTX44Copy( const Mtx44 src, Mtx44 dst )
{

	assert((src != 0) );
	assert((dst != 0) );

	if( src == dst )
	{
		return;
	}

	dst[0][0] = src[0][0]; dst[0][1] = src[0][1]; dst[0][2] = src[0][2]; dst[0][3] = src[0][3];

	dst[1][0] = src[1][0]; dst[1][1] = src[1][1]; dst[1][2] = src[1][2]; dst[1][3] = src[1][3];

	dst[2][0] = src[2][0]; dst[2][1] = src[2][1]; dst[2][2] = src[2][2]; dst[2][3] = src[2][3];

	dst[3][0] = src[3][0]; dst[3][1] = src[3][1]; dst[3][2] = src[3][2]; dst[3][3] = src[3][3];

}

void C_MTX44Concat( const Mtx44 a, const Mtx44 b, Mtx44 ab )
{
	Mtx44       mTmp;
	Mtx44Ptr    m;

	assert((a  != 0));
	assert((b  != 0));
	assert((ab != 0));

	if( (ab == a) || (ab == b) )
	{
		m = mTmp;
	}
	else
	{
		m = ab;
	}

	m[0][0] = a[0][0]*b[0][0] + a[0][1]*b[1][0] + a[0][2]*b[2][0] + a[0][3]*b[3][0];
	m[0][1] = a[0][0]*b[0][1] + a[0][1]*b[1][1] + a[0][2]*b[2][1] + a[0][3]*b[3][1];
	m[0][2] = a[0][0]*b[0][2] + a[0][1]*b[1][2] + a[0][2]*b[2][2] + a[0][3]*b[3][2];
	m[0][3] = a[0][0]*b[0][3] + a[0][1]*b[1][3] + a[0][2]*b[2][3] + a[0][3]*b[3][3];

	m[1][0] = a[1][0]*b[0][0] + a[1][1]*b[1][0] + a[1][2]*b[2][0] + a[1][3]*b[3][0];
	m[1][1] = a[1][0]*b[0][1] + a[1][1]*b[1][1] + a[1][2]*b[2][1] + a[1][3]*b[3][1];
	m[1][2] = a[1][0]*b[0][2] + a[1][1]*b[1][2] + a[1][2]*b[2][2] + a[1][3]*b[3][2];
	m[1][3] = a[1][0]*b[0][3] + a[1][1]*b[1][3] + a[1][2]*b[2][3] + a[1][3]*b[3][3];

	m[2][0] = a[2][0]*b[0][0] + a[2][1]*b[1][0] + a[2][2]*b[2][0] + a[2][3]*b[3][0];
	m[2][1] = a[2][0]*b[0][1] + a[2][1]*b[1][1] + a[2][2]*b[2][1] + a[2][3]*b[3][1];
	m[2][2] = a[2][0]*b[0][2] + a[2][1]*b[1][2] + a[2][2]*b[2][2] + a[2][3]*b[3][2];
	m[2][3] = a[2][0]*b[0][3] + a[2][1]*b[1][3] + a[2][2]*b[2][3] + a[2][3]*b[3][3];

	m[3][0] = a[3][0]*b[0][0] + a[3][1]*b[1][0] + a[3][2]*b[2][0] + a[3][3]*b[3][0];
	m[3][1] = a[3][0]*b[0][1] + a[3][1]*b[1][1] + a[3][2]*b[2][1] + a[3][3]*b[3][1];
	m[3][2] = a[3][0]*b[0][2] + a[3][1]*b[1][2] + a[3][2]*b[2][2] + a[3][3]*b[3][2];
	m[3][3] = a[3][0]*b[0][3] + a[3][1]*b[1][3] + a[3][2]*b[2][3] + a[3][3]*b[3][3];

	if(m == mTmp)
	{
		C_MTX44Copy( mTmp, ab );
	}

}

void C_MTX44Transpose ( const Mtx44 src, Mtx44 xPose )
{
	Mtx44       mTmp;
	Mtx44Ptr    m;

	assert((src   != 0));
	assert((xPose != 0));

	if(src == xPose)
	{
		m = mTmp;
	}
	else
	{
		m = xPose;
	}

	m[0][0] = src[0][0];    m[0][1] = src[1][0];    m[0][2] = src[2][0];    m[0][3] = src[3][0];
	m[1][0] = src[0][1];    m[1][1] = src[1][1];    m[1][2] = src[2][1];    m[1][3] = src[3][1];
	m[2][0] = src[0][2];    m[2][1] = src[1][2];    m[2][2] = src[2][2];    m[2][3] = src[3][2];
	m[3][0] = src[0][3];    m[3][1] = src[1][3];    m[3][2] = src[2][3];    m[3][3] = src[3][3];

	if( m == mTmp )
	{
		MTX44Copy( mTmp, xPose );
	}
}

u32 C_MTX44Inverse( const Mtx44 src, Mtx44 inv )
{
	Mtx44       gjm;
	s32         i, j, k;
	f32         w;

	assert((src != 0));
	assert((inv != 0));

	MTX44Copy(src, gjm);
	MTX44Identity(inv);

	for ( i = 0 ; i < 4 ; ++i )
	{
		f32 max = 0.0f;
		s32 swp = i;

		for( k = i ; k < 4 ; k++ )
		{
			f32 ftmp;
			ftmp = fabsf(gjm[k][i]);
			if ( ftmp > max )
			{
				max = ftmp;
				swp = k;
			}
		}

		if ( max == 0.0f )
		{
			return 0;
		}

		if( swp != i )
		{
			for ( k = 0 ; k < 4 ; k++ )
			{
				{ f32 swap = gjm[i][k]; gjm[i][k] = gjm[swp][k]; gjm[swp][k] = swap; }
				{ f32 swap = inv[i][k]; inv[i][k] = inv[swp][k]; inv[swp][k] = swap; }
			}
		}

		w = 1.0F / gjm[i][i];
		for ( j = 0 ; j < 4 ; ++j )
		{
			gjm[i][j] *= w;
			inv[i][j] *= w;
		}

		for ( k = 0 ; k < 4 ; ++k )
		{
			if ( k == i ) {
				continue;
			}

			w = gjm[k][i];
			for ( j = 0 ; j < 4 ; ++j )
			{
				gjm[k][j] -= gjm[i][j] * w;
				inv[k][j] -= inv[i][j] * w;
			}
		}

	}

	return 1;
}

void C_MTX44Trans ( Mtx44 m, f32 xT, f32 yT, f32 zT )
{
	assert((m != 0));

	m[0][0] = 1.0f;     m[0][1] = 0.0f;  m[0][2] = 0.0f;  m[0][3] =  xT;
	m[1][0] = 0.0f;     m[1][1] = 1.0f;  m[1][2] = 0.0f;  m[1][3] =  yT;
	m[2][0] = 0.0f;     m[2][1] = 0.0f;  m[2][2] = 1.0f;  m[2][3] =  zT;
	m[3][0] = 0.0f;     m[3][1] = 0.0f;  m[3][2] = 0.0f;  m[3][3] =  1.0f;

}

void C_MTX44TransApply ( const Mtx44 src, Mtx44 dst, f32 xT, f32 yT, f32 zT )
{
	assert((src != 0));
	assert((dst != 0));

	if ( src != dst )
	{
		dst[0][0] = src[0][0];    dst[0][1] = src[0][1];    dst[0][2] = src[0][2];
		dst[1][0] = src[1][0];    dst[1][1] = src[1][1];    dst[1][2] = src[1][2];
		dst[2][0] = src[2][0];    dst[2][1] = src[2][1];    dst[2][2] = src[2][2];
		dst[3][0] = src[3][0];    dst[3][1] = src[3][1];    dst[3][2] = src[3][2];
		dst[3][3] = src[3][3];
	}

	dst[0][3] = src[0][3] + xT;
	dst[1][3] = src[1][3] + yT;
	dst[2][3] = src[2][3] + zT;

}

void C_MTX44Scale ( Mtx44 m, f32 xS, f32 yS, f32 zS )
{
	assert((m != 0));

	m[0][0] = xS;      m[0][1] = 0.0f;  m[0][2] = 0.0f;  m[0][3] = 0.0f;
	m[1][0] = 0.0f;    m[1][1] = yS;    m[1][2] = 0.0f;  m[1][3] = 0.0f;
	m[2][0] = 0.0f;    m[2][1] = 0.0f;  m[2][2] = zS;    m[2][3] = 0.0f;
	m[3][0] = 0.0f;    m[3][1] = 0.0f;  m[3][2] = 0.0f;  m[3][3] = 1.0f;
}

void C_MTX44ScaleApply ( const Mtx44 src, Mtx44 dst, f32 xS, f32 yS, f32 zS )
{
	assert((src != 0));
	assert((dst != 0));

	dst[0][0] = src[0][0] * xS;     dst[0][1] = src[0][1] * xS;
	dst[0][2] = src[0][2] * xS;     dst[0][3] = src[0][3] * xS;

	dst[1][0] = src[1][0] * yS;     dst[1][1] = src[1][1] * yS;
	dst[1][2] = src[1][2] * yS;     dst[1][3] = src[1][3] * yS;

	dst[2][0] = src[2][0] * zS;     dst[2][1] = src[2][1] * zS;
	dst[2][2] = src[2][2] * zS;     dst[2][3] = src[2][3] * zS;

	dst[3][0] = src[3][0] ; dst[3][1] = src[3][1];
	dst[3][2] = src[3][2] ; dst[3][3] = src[3][3];
}

void C_MTX44RotRad ( Mtx44 m, char axis, f32 rad )
{

	f32 sinA, cosA;

	assert((m != 0));

	sinA = sinf(rad);
	cosA = cosf(rad);

	C_MTX44RotTrig( m, axis, sinA, cosA );
}

void C_MTX44RotTrig ( Mtx44 m, char axis, f32 sinA, f32 cosA )
{
	assert((m != 0));

	axis |= 0x20;
	switch(axis)
	{

	case 'x':
		m[0][0] =  1.0f;  m[0][1] =  0.0f;    m[0][2] =  0.0f;  m[0][3] = 0.0f;
		m[1][0] =  0.0f;  m[1][1] =  cosA;    m[1][2] = -sinA;  m[1][3] = 0.0f;
		m[2][0] =  0.0f;  m[2][1] =  sinA;    m[2][2] =  cosA;  m[2][3] = 0.0f;
		m[3][0] =  0.0f;  m[3][1] =  0.0f;    m[3][2] =  0.0f;  m[3][3] = 1.0f;
		break;

	case 'y':
		m[0][0] =  cosA;  m[0][1] =  0.0f;    m[0][2] =  sinA;  m[0][3] = 0.0f;
		m[1][0] =  0.0f;  m[1][1] =  1.0f;    m[1][2] =  0.0f;  m[1][3] = 0.0f;
		m[2][0] = -sinA;  m[2][1] =  0.0f;    m[2][2] =  cosA;  m[2][3] = 0.0f;
		m[3][0] =  0.0f;  m[3][1] =  0.0f;    m[3][2] =  0.0f;  m[3][3] = 1.0f;
		break;

	case 'z':
		m[0][0] =  cosA;  m[0][1] = -sinA;    m[0][2] =  0.0f;  m[0][3] = 0.0f;
		m[1][0] =  sinA;  m[1][1] =  cosA;    m[1][2] =  0.0f;  m[1][3] = 0.0f;
		m[2][0] =  0.0f;  m[2][1] =  0.0f;    m[2][2] =  1.0f;  m[2][3] = 0.0f;
		m[3][0] =  0.0f;  m[3][1] =  0.0f;    m[3][2] =  0.0f;  m[3][3] = 1.0f;
		break;

	default:
		assert(0);
		break;
	}
}

void C_MTX44RotAxisRad( Mtx44 m, const Vec *axis, f32 rad )
{
	Vec vN;
	f32 s, c;
	f32 t;
	f32 x, y, z;
	f32 xSq, ySq, zSq;

	assert((m    != 0));
	assert((axis != 0));

	s = sinf(rad);
	c = cosf(rad);
	t = 1.0f - c;

	C_VECNormalize( axis, &vN );

	x = vN.x;
	y = vN.y;
	z = vN.z;

	xSq = x * x;
	ySq = y * y;
	zSq = z * z;

	m[0][0] = ( t * xSq )   + ( c );
	m[0][1] = ( t * x * y ) - ( s * z );
	m[0][2] = ( t * x * z ) + ( s * y );
	m[0][3] =    0.0f;

	m[1][0] = ( t * x * y ) + ( s * z );
	m[1][1] = ( t * ySq )   + ( c );
	m[1][2] = ( t * y * z ) - ( s * x );
	m[1][3] =    0.0f;

	m[2][0] = ( t * x * z ) - ( s * y );
	m[2][1] = ( t * y * z ) + ( s * x );
	m[2][2] = ( t * zSq )   + ( c );
	m[2][3] =    0.0f;

	m[3][0] = 0.0f;
	m[3][1] = 0.0f;
	m[3][2] = 0.0f;
	m[3][3] = 1.0f;

}

void C_MTX44MultVec ( const Mtx44 m, const Vec *src, Vec *dst )
{
	Vec vTmp;
	f32 w;

	assert((m   != 0));
	assert((src != 0));
	assert((dst != 0));

	vTmp.x = m[0][0]*src->x + m[0][1]*src->y + m[0][2]*src->z + m[0][3];
	vTmp.y = m[1][0]*src->x + m[1][1]*src->y + m[1][2]*src->z + m[1][3];
	vTmp.z = m[2][0]*src->x + m[2][1]*src->y + m[2][2]*src->z + m[2][3];
	w      = m[3][0]*src->x + m[3][1]*src->y + m[3][2]*src->z + m[3][3];
	w = 1.0f/w;

	dst->x = vTmp.x * w;
	dst->y = vTmp.y * w;
	dst->z = vTmp.z * w;
}

void C_MTX44MultVecArray ( const Mtx44 m, const Vec *srcBase, Vec *dstBase, u32 count )
{
	u32 i;
	Vec vTmp;
	f32 w;

	assert((m       != 0));
	assert((srcBase != 0));
	assert((dstBase != 0));

	for(i=0; i< count; i++)
	{

		vTmp.x = m[0][0]*srcBase->x + m[0][1]*srcBase->y + m[0][2]*srcBase->z + m[0][3];
		vTmp.y = m[1][0]*srcBase->x + m[1][1]*srcBase->y + m[1][2]*srcBase->z + m[1][3];
		vTmp.z = m[2][0]*srcBase->x + m[2][1]*srcBase->y + m[2][2]*srcBase->z + m[2][3];
		w      = m[3][0]*srcBase->x + m[3][1]*srcBase->y + m[3][2]*srcBase->z + m[3][3];
		w = 1.0f/w;

		dstBase->x = vTmp.x * w;
		dstBase->y = vTmp.y * w;
		dstBase->z = vTmp.z * w;

		srcBase++;
		dstBase++;
	}
}

void C_MTX44MultVecSR ( const Mtx44 m, const Vec *src, Vec *dst )
{
	Vec vTmp;

	assert((m   != 0));
	assert((src != 0));
	assert((dst != 0));

	vTmp.x = m[0][0]*src->x + m[0][1]*src->y + m[0][2]*src->z;
	vTmp.y = m[1][0]*src->x + m[1][1]*src->y + m[1][2]*src->z;
	vTmp.z = m[2][0]*src->x + m[2][1]*src->y + m[2][2]*src->z;

	dst->x = vTmp.x;
	dst->y = vTmp.y;
	dst->z = vTmp.z;
}

void C_MTX44MultVecArraySR ( const Mtx44 m, const Vec *srcBase, Vec *dstBase, u32 count )
{
	u32 i;
	Vec vTmp;

	assert((m       != 0));
	assert((srcBase != 0));
	assert((dstBase != 0));

	for ( i = 0; i < count; i ++ )
	{

		vTmp.x = m[0][0]*srcBase->x + m[0][1]*srcBase->y + m[0][2]*srcBase->z;
		vTmp.y = m[1][0]*srcBase->x + m[1][1]*srcBase->y + m[1][2]*srcBase->z;
		vTmp.z = m[2][0]*srcBase->x + m[2][1]*srcBase->y + m[2][2]*srcBase->z;

		dstBase->x = vTmp.x;
		dstBase->y = vTmp.y;
		dstBase->z = vTmp.z;

		srcBase++;
		dstBase++;
	}
}
