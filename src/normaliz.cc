/*
 * NormalizInterface: GAP wrapper for normaliz
 * Copyright (C) 2014  Sebastian Gutsche, Max Horn, Christof Söger
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/*
#! @Chapter Functions
#! @Section YOU FORGOT TO SET A SECTION
*/

#include "normaliz.h"
#include "libnormaliz/map_operations.h"

#include <vector>
#include <iostream>

using libnormaliz::Cone;
//using libnormaliz::ConeProperty;
using libnormaliz::ConeProperties;
using libnormaliz::Sublattice_Representation;
using libnormaliz::Type::InputType;

using std::map;
using std::vector;
using std::string;
using std::pair;

using std::cerr;
using std::endl;

Obj TheTypeNormalizLongIntCone;
Obj TheTypeNormalizGMPCone;


Obj NewCone(Cone<long>* C)
{
    Obj o;
    o = NewBag(T_NORMALIZ, 2*sizeof(Obj));

    ADDR_OBJ(o)[0] = (Obj)NMZ_LONG_INT_CONE_TYPE;
    SET_CONE<long>(o, C);
    return o;
}

Obj NewCone(Cone<mpz_class>* C)
{
    Obj o;
    o = NewBag(T_NORMALIZ, 2 * sizeof(Obj));

    ADDR_OBJ(o)[0] = (Obj)NMZ_GMP_CONE_TYPE;
    SET_CONE<mpz_class>(o, C);
    return o;
}

/* Free function */
void NormalizFreeFunc(Obj o)
{
    if (IS_LONG_INT_CONE(o))
        delete GET_CONE<long>(o);
    else
        delete GET_CONE<mpz_class>(o);
}

/* Type object function for the object */
Obj NormalizTypeFunc(Obj o)
{
    if (IS_LONG_INT_CONE(o))
        return TheTypeNormalizLongIntCone;
    else
        return TheTypeNormalizGMPCone;
}

static Obj MpzToGAP(const mpz_t x)
{
    Obj res;
    Int size = x->_mp_size;
    int sign;
    if (size == 0) {
        return INTOBJ_INT(0);
    } else if (size < 0) {
        size = -size;
        sign = -1;
    } else {
        sign = +1;
    }
#ifdef SYS_IS_64_BIT
    if (size == 1) {
        if (sign > 0)
            return ObjInt_UInt(x->_mp_d[0]);
        else
            return AInvInt(ObjInt_UInt(x->_mp_d[0]));
    }
#endif
    size = sizeof(mp_limb_t) * size;
    if (sign > 0)
        res = NewBag(T_INTPOS, size);
    else
        res = NewBag(T_INTNEG, size);
    memcpy(ADDR_INT(res), x->_mp_d, size);
    return res;
}

static inline Obj MpzClassToGAP(const mpz_class& x)
{
    return MpzToGAP(x.get_mpz_t());
}

static Obj MpqClassToGAP(const mpq_class& x)
{
    Obj num = MpzClassToGAP(x.get_num());
    Obj den = MpzClassToGAP(x.get_den());
    return QUO(num, den);
}

template<typename Integer>
Obj NmzIntToGAP(Integer x)
{
    return Integer::unimplemented_function;
}

template<>
Obj NmzIntToGAP(libnormaliz::key_t x)   // key_t = unsigned int
{
    return ObjInt_UInt(x);
}

template<>
Obj NmzIntToGAP(size_t x)               // size_t = unsigned long
{
    return ObjInt_UInt(x);
}

template<>
Obj NmzIntToGAP(long x)
{
    return ObjInt_Int(x);
}

template<>
Obj NmzIntToGAP(mpz_class x)
{
    return MpzClassToGAP(x);
}



template<typename Integer>
bool GAPIntToNmz(Obj x, Integer &out)
{
    return Integer::unimplemented_function;
}

template<>
bool GAPIntToNmz(Obj x, long &out)
{
    if (IS_INTOBJ(x)) {
        out = INT_INTOBJ(x);
        return true;
    } else if (TNUM_OBJ(x) == T_INTPOS || TNUM_OBJ(x) == T_INTNEG) {
        UInt size = SIZE_INT(x);
        if (size == 1) {
            out = *ADDR_INT(x);
            if (out < 0)
                return false;   // overflow
            if (TNUM_OBJ(x) == T_INTNEG)
                out = -out;
                return true;
        }
    }
    return false;
}

template<>
bool GAPIntToNmz(Obj x, mpz_class &out)
{
    if (IS_INTOBJ(x)) {
        out = (int)INT_INTOBJ(x);
        return true;
    } else if (TNUM_OBJ(x) == T_INTPOS || TNUM_OBJ(x) == T_INTNEG) {
        mpz_ptr m = out.get_mpz_t();
        UInt size = SIZE_INT(x);
        mpz_realloc2(m, size * GMP_NUMB_BITS);
        memcpy(m->_mp_d, ADDR_INT(x), sizeof(mp_limb_t) * size);
        m->_mp_size = (TNUM_OBJ(x) == T_INTPOS) ? (Int)size : - (Int)size;
        return true;
    }
    return false;
}

template<typename Integer>
static bool GAPIntVectorToNmz(vector<Integer>& out, Obj V)
{
    if (!IS_PLIST(V) || !IS_DENSE_LIST(V))
        return false;
    const int n = LEN_PLIST(V);
    out.resize(n);
    for (int i = 0; i < n; ++i) {
        Obj tmp = ELM_PLIST(V, i+1);
        if (!GAPIntToNmz(tmp, out[i]))
            return false;
    }
    return true;
}

template<typename Integer>
static bool GAPIntMatrixToNmz(vector< vector<Integer> >& out, Obj M)
{
    if (!IS_PLIST(M) || !IS_DENSE_LIST(M))
        return false;
    const int nr = LEN_PLIST(M);
    out.resize(nr);
    for (int i = 0; i < nr; ++i) {
        bool okay = GAPIntVectorToNmz(out[i], ELM_PLIST(M, i+1));
        if (!okay)
            return false;
    }
    return true;
}

template<typename Integer>
static Obj NmzVectorToGAP(const vector<Integer>& in)
{
    Obj M;
    const size_t n = in.size();
    M = NEW_PLIST((n > 0) ? T_PLIST_CYC : T_PLIST, n);
    SET_LEN_PLIST(M, n);
    for (size_t i = 0; i < n; ++i) {
        SET_ELM_PLIST(M, i+1, NmzIntToGAP(in[i]));
        CHANGED_BAG( M );
    }
    return M;
}

template<typename Integer>
static Obj NmzMatrixToGAP(const vector< vector<Integer> >& in)
{
    Obj M;
    const size_t n = in.size();
    M = NEW_PLIST(T_PLIST, n);
    SET_LEN_PLIST(M, n);
    for (size_t i = 0; i < n; ++i) {
        SET_ELM_PLIST(M, i+1, NmzVectorToGAP(in[i]));
        CHANGED_BAG( M );
    }
    CHANGED_BAG( M );
    return M;
}

static Obj NmzHilbertSeriesToGAP(const libnormaliz::HilbertSeries& HS)
{
    Obj ret;
    ret = NEW_PLIST(T_PLIST, 2);
    SET_LEN_PLIST(ret, 2);
    AssPlist(ret, 1, NmzVectorToGAP(HS.getNum()));
    AssPlist(ret, 2, NmzVectorToGAP(libnormaliz::to_vector(HS.getDenom())));
    return ret;
}

static Obj NmzHilbertFunctionToGAP(const libnormaliz::HilbertSeries& HS)
{
    Obj ret;
    vector< vector<mpz_class> > HQ = HS.getHilbertQuasiPolynomial();
    const size_t n = HS.getPeriod();
    ret = NEW_PLIST(T_PLIST, n+1);
    SET_LEN_PLIST(ret, n+1);

    for (size_t i = 0; i < n; ++i) {
        SET_ELM_PLIST(ret, i+1, NmzVectorToGAP(HQ[i]));
        CHANGED_BAG( ret );
    }
    AssPlist(ret, n+1, NmzIntToGAP(HS.getHilbertQuasiPolynomialDenom()));
    return ret;
}

template<typename Integer>
static Obj NmzTriangleListToGAP(const vector< pair<vector<libnormaliz::key_t>, Integer> >& in)
{
    Obj M;
    const size_t n = in.size();
    M = NEW_PLIST(T_PLIST, n);
    SET_LEN_PLIST(M, n);
    for (size_t i = 0; i < n; ++i) {
        // convert the pair
        Obj pair = NEW_PLIST(T_PLIST, 2);
        SET_LEN_PLIST(pair, 2);
        SET_ELM_PLIST(pair, 1, NmzVectorToGAP<libnormaliz::key_t>(in[i].first));
        SET_ELM_PLIST(pair, 2, NmzIntToGAP(in[i].second));
        CHANGED_BAG( pair );
        
        SET_ELM_PLIST(M, i+1, pair);
        CHANGED_BAG( M );
    }
    CHANGED_BAG( M );
    return M;
}


template<typename Integer>
static Obj _NmzCone(Obj input_list)
{
    map <InputType, vector< vector<Integer> > > input;
    const int n = LEN_PLIST(input_list);
    if (n&1) {
        cerr << "Input list must have even number of elements" << endl;
        return Fail;
    }
    for (int i = 0; i < n; i += 2) {
        Obj type = ELM_PLIST(input_list, i+1);
        if (!IS_STRING_REP(type)) {
            cerr << "Element " << i+1 << " of the input list must be a type string" << endl;
            return Fail;
        }
        string type_str(CSTR_STRING(type));

        Obj M = ELM_PLIST(input_list, i+2);
        vector<vector<Integer> > Mat;
        bool okay = GAPIntMatrixToNmz(Mat, M);
        if (!okay) {
            cerr << "Element " << i+2 << " of the input list must integer matrix" << endl;
            return Fail;
        }

        input[libnormaliz::to_type(type_str)] = Mat;
    }

    Cone<Integer>* C = new Cone<Integer>(input);
    Obj Cone = NewCone(C);
    return Cone;

}

Obj NmzLongIntCone(Obj self, Obj input_list)
{
    FUNC_BEGIN
    if (!IS_PLIST(input_list) || !IS_DENSE_LIST(input_list))
        ErrorQuit("Input argument must be a list", 0, 0);

    return _NmzCone<long>(input_list);

    FUNC_END
}

Obj NmzGMPCone(Obj self, Obj input_list)
{
    FUNC_BEGIN
    if (!IS_PLIST(input_list) || !IS_DENSE_LIST(input_list))
        ErrorQuit("Input argument must be a list", 0, 0);

    return _NmzCone<mpz_class>(input_list);

    FUNC_END
}

Obj _NmzCompute(Obj self, Obj cone, Obj to_compute)
{
    FUNC_BEGIN
    if (!IS_CONE(cone))
        ErrorQuit("<cone> must be a normaliz cone", 0, 0);
    if (!IS_PLIST(to_compute) || !IS_DENSE_LIST(to_compute))
        ErrorQuit("<props> must be a list of strings", 0, 0);

    ConeProperties propsToCompute;
    // we have a list
    const int n = LEN_PLIST(to_compute);

    for (int i = 0; i < n; ++i) {
        Obj prop = ELM_PLIST(to_compute, i+1);
        if (!IS_STRING_REP(prop)) {
            cerr << "Element " << i+1 << " of the input list must be a type string";
            return Fail;
        }
        string prop_str(CSTR_STRING(prop));
        propsToCompute.set( libnormaliz::toConeProperty(prop_str) );
    }

    ConeProperties notComputed;

    if (IS_LONG_INT_CONE(cone)) {
        Cone<long>* C = GET_CONE<long>(cone);
        notComputed = C->compute(propsToCompute);
    } else {
        Cone<mpz_class>* C = GET_CONE<mpz_class>(cone);
        notComputed = C->compute(propsToCompute);
    }

    // Cone.compute returns the not computed properties
    // we return a bool, true when everything requested was computed
    return notComputed.none() ? True : False;
    FUNC_END
}

Obj NmzHasConeProperty(Obj self, Obj cone, Obj prop)
{
    FUNC_BEGIN

    if (!IS_CONE(cone))
        ErrorQuit("<cone> must be a normaliz cone", 0, 0);
    if (!IS_STRING_REP(prop))
        ErrorQuit("<prop> must be a string", 0, 0);

    libnormaliz::ConeProperty::Enum p = libnormaliz::toConeProperty(CSTR_STRING(prop));

    if (IS_LONG_INT_CONE(cone)) {
        Cone<long>* C = GET_CONE<long>(cone);
        return C->isComputed(p) ? True : False;
    } else {
        Cone<mpz_class>* C = GET_CONE<mpz_class>(cone);
        return C->isComputed(p) ? True : False;
    }

    FUNC_END
}

/*
#! @Section Use a NmzCone
#! @Arguments cone
#! @Returns a list of strings representing the known (computed) cone properties
#! @Description
#! Given a Normaliz cone object, return a list of all properties already
#! computed for the cone.
#!
#! TODO: add an example
DeclareGlobalFunction("NmzKnownConeProperties");
*/
Obj NmzKnownConeProperties(Obj self, Obj cone)
{
    FUNC_BEGIN

    if (!IS_CONE(cone))
        ErrorQuit("<cone> must be a normaliz cone", 0, 0);

    size_t n = 0;
    Obj M = NEW_PLIST(T_PLIST, libnormaliz::ConeProperty::EnumSize);

    // FIXME: This code could be a lot simpler if there was
    // a Cone method for reading the value of is_Computed.
    for (int i = 0; i < libnormaliz::ConeProperty::EnumSize; ++i) {
        libnormaliz::ConeProperty::Enum p = (libnormaliz::ConeProperty::Enum)i;

        bool isComputed;

        if (IS_LONG_INT_CONE(cone)) {
            Cone<long>* C = GET_CONE<long>(cone);
            isComputed = C->isComputed(p);
        } else {
            Cone<mpz_class>* C = GET_CONE<mpz_class>(cone);
            isComputed = C->isComputed(p);
        }

        if (isComputed) {
            string prop_name(libnormaliz::toString(p));

            Obj prop_name_gap;
            C_NEW_STRING(prop_name_gap, prop_name.size(), prop_name.c_str());

            n++;    // Increment counter
            SET_ELM_PLIST(M, n, prop_name_gap);
            CHANGED_BAG(M);
        }
    }
    SET_LEN_PLIST(M, n);
    return M;

    FUNC_END
}

template<typename Integer>
static Obj _NmzConePropertyImpl(Obj cone, Obj prop)
{
    Cone<Integer>* C = GET_CONE<Integer>(cone);
    libnormaliz::ConeProperty::Enum p = libnormaliz::toConeProperty(CSTR_STRING(prop));

    ConeProperties notComputed = C->compute(ConeProperties(p));
    if (notComputed.any()) {
        return Fail;
    }

    switch (p) {
    case libnormaliz::ConeProperty::Generators:
        return NmzMatrixToGAP(C->getGenerators());

    case libnormaliz::ConeProperty::ExtremeRays:
        return NmzMatrixToGAP(C->getExtremeRays());

    case libnormaliz::ConeProperty::VerticesOfPolyhedron:
        return NmzMatrixToGAP(C->getVerticesOfPolyhedron());

    case libnormaliz::ConeProperty::SupportHyperplanes:
        return NmzMatrixToGAP(C->getSupportHyperplanes());

    case libnormaliz::ConeProperty::TriangulationSize:
        return ObjInt_UInt(C->getTriangulationSize());

    case libnormaliz::ConeProperty::TriangulationDetSum:
        return NmzIntToGAP(C->getTriangulationDetSum());

    case libnormaliz::ConeProperty::Triangulation:
        return NmzTriangleListToGAP<Integer>(C->getTriangulation());

    case libnormaliz::ConeProperty::Multiplicity:
        {
        mpq_class mult = C->getMultiplicity();
        return MpqClassToGAP(mult);
        }

    case libnormaliz::ConeProperty::Shift:
        return NmzIntToGAP(C->getShift());

    case libnormaliz::ConeProperty::AffineDim:
        return NmzIntToGAP(C->getAffineDim());

    case libnormaliz::ConeProperty::RecessionRank:
        return NmzIntToGAP(C->getRecessionRank());

    case libnormaliz::ConeProperty::ModuleRank:
        return NmzIntToGAP(C->getModuleRank());

    case libnormaliz::ConeProperty::HilbertBasis:
        return NmzMatrixToGAP(C->getHilbertBasis());

    case libnormaliz::ConeProperty::ModuleGenerators:
        return NmzMatrixToGAP(C->getModuleGenerators());

    case libnormaliz::ConeProperty::Deg1Elements:
        return NmzMatrixToGAP(C->getDeg1Elements());

    case libnormaliz::ConeProperty::HilbertSeries:
        return NmzHilbertSeriesToGAP(C->getHilbertSeries());
        break;

    case libnormaliz::ConeProperty::HilbertFunction:
        return NmzHilbertFunctionToGAP(C->getHilbertSeries());
        break;

    case libnormaliz::ConeProperty::Grading:
        {
        vector<Integer> grad = C->getGrading();
        grad.push_back(C->getGradingDenom());
        return NmzVectorToGAP(grad);
        }

    case libnormaliz::ConeProperty::Dehomogenization:
        return NmzVectorToGAP(C->getDehomogenization());

    case libnormaliz::ConeProperty::IsPointed:
        return C->isPointed() ? True : False;

    case libnormaliz::ConeProperty::IsDeg1ExtremeRays:
        return C->isDeg1ExtremeRays() ? True : False;

    case libnormaliz::ConeProperty::IsDeg1HilbertBasis:
        return C->isDeg1HilbertBasis() ? True : False;

    case libnormaliz::ConeProperty::IsIntegrallyClosed:
        return C->isIntegrallyClosed() ? True : False;

    case libnormaliz::ConeProperty::OriginalMonoidGenerators:
        return NmzMatrixToGAP(C->getOriginalMonoidGenerators());

    case libnormaliz::ConeProperty::ReesPrimary:
        return C->isReesPrimary() ? True : False;

    case libnormaliz::ConeProperty::ReesPrimaryMultiplicity:
        return NmzIntToGAP(C->getReesPrimaryMultiplicity());

    case libnormaliz::ConeProperty::InclusionExclusionData:
        return NmzTriangleListToGAP<long>(C->getInclusionExclusionData());

    // the following is special and we do not support a conversion
    // (if you really need this, contact the developers)
    case libnormaliz::ConeProperty::StanleyDec:
        //C->getStanleyDec();
        break;

    // the following are not yet supported, but we'd like to
    case libnormaliz::ConeProperty::IsDeg1Generated:        // TODO
    case libnormaliz::ConeProperty::GeneratorsOfToricRing:  // TODO
    case libnormaliz::ConeProperty::ExcludedFaces:          // TODO
    case libnormaliz::ConeProperty::ApproximateRatPolytope: // TODO

#ifdef NORMALIZ_3
    case libnormaliz::ConeProperty::BottomDecomposition:    // TODO
    case libnormaliz::ConeProperty::KeepOrder:              // TODO
    case libnormaliz::ConeProperty::ClassGroup:             // TODO
#endif

//  the following properties are compute options and do not return anything
    case libnormaliz::ConeProperty::DualMode:
    case libnormaliz::ConeProperty::DefaultMode:
        return True;

    default:
        // Case not handled. Should signal an error
        break;
    }

    return Fail;
}

Obj _NmzConeProperty(Obj self, Obj cone, Obj prop)
{
    FUNC_BEGIN

    if (!IS_CONE(cone))
        ErrorQuit("<cone> must be a normaliz cone", 0, 0);
    if (!IS_STRING_REP(prop))
        ErrorQuit("<prop> must be a string", 0, 0);

    if (IS_LONG_INT_CONE(cone)) {
        return _NmzConePropertyImpl<long>(cone, prop);
    } else {
        return _NmzConePropertyImpl<mpz_class>(cone, prop);
    }

    FUNC_END
}



/*
#! @Section Use a NmzCone
#! @Arguments verboseFlag
#! @Returns the previous verbosity
#! @Description
#! Set the global verbosity state in libnormaliz
DeclareGlobalFunction("NmzVerbose");
*/
Obj NmzVerbose(Obj self, Obj value)
{
    FUNC_BEGIN
    if (value != True && value != False)
        ErrorQuit("<value> must be a boolean value", 0, 0);
    bool old_value = libnormaliz::verbose;
    libnormaliz::verbose = (value == True);
    return old_value ? True : False;
    FUNC_END
}

/*
#! @Section Cone properties
*/

/*
#! @Arguments cone
#! @Returns the embedding dimension of the cone
#! @Description
DeclareGlobalFunction("NmzEmbeddingDimension");
*/
Obj NmzEmbeddingDimension(Obj self, Obj cone)
{
    FUNC_BEGIN
    if (!IS_CONE(cone))
        ErrorQuit("<cone> must be a normaliz cone", 0, 0);
    if (IS_LONG_INT_CONE(cone)) {
        Cone<long>* C = GET_CONE<long>(cone);
        return NmzIntToGAP(C->getEmbeddingDim());
    } else {
        Cone<mpz_class>* C = GET_CONE<mpz_class>(cone);
        return NmzIntToGAP(C->getEmbeddingDim());
    }
    FUNC_END
}

template<typename Integer>
static Obj _NmzBasisChangeIntern(Obj cone)
{
    Cone<Integer>* C = GET_CONE<Integer>(cone);
    Sublattice_Representation<Integer> bc = C->getBasisChange();

    // TODO: return a record instead of an array. For now,
    // we use an array because it is simpler.
    Obj res = NEW_PLIST(T_PLIST, 6);
    SET_LEN_PLIST(res, 6);
    AssPlist(res, 1, NmzIntToGAP(bc.get_dim()));
    AssPlist(res, 2, NmzIntToGAP(bc.get_rank()));
    AssPlist(res, 3, NmzIntToGAP(bc.get_index()));
    AssPlist(res, 4, NmzMatrixToGAP(bc.get_A().get_elements()));
    AssPlist(res, 5, NmzMatrixToGAP(bc.get_B().get_elements()));
    AssPlist(res, 6, NmzIntToGAP(bc.get_c()));
    // bc.get_congruences() is already covered by NmzCongruences
    return res;
}

Obj _NmzBasisChange(Obj self, Obj cone)
{
    FUNC_BEGIN
    if (!IS_CONE(cone))
        ErrorQuit("<cone> must be a normaliz cone", 0, 0);
    if (IS_LONG_INT_CONE(cone)) {
        return _NmzBasisChangeIntern<long>(cone);
    } else {
        return _NmzBasisChangeIntern<mpz_class>(cone);
    }
    FUNC_END
}

/*
#! @Arguments cone
#! @Returns TODO
#! @Description
#! TODO
DeclareGlobalFunction("NmzRank");
*/
Obj NmzRank(Obj self, Obj cone)
{
    FUNC_BEGIN
    if (!IS_CONE(cone))
        ErrorQuit("<cone> must be a normaliz cone", 0, 0);
    if (IS_LONG_INT_CONE(cone)) {
        Cone<long>* C = GET_CONE<long>(cone);
        return NmzIntToGAP(C->getBasisChange().get_rank());
    } else {
        Cone<mpz_class>* C = GET_CONE<mpz_class>(cone);
        return NmzIntToGAP(C->getBasisChange().get_rank());
    }
    FUNC_END
}

/*
#! @Arguments cone
#! @Returns TODO
#! @Description
#! TODO
DeclareGlobalFunction("NmzIsInhomogeneous");
*/
Obj NmzIsInhomogeneous(Obj self, Obj cone)
{
    FUNC_BEGIN
    if (!IS_CONE(cone))
        ErrorQuit("<cone> must be a normaliz cone", 0, 0);
    if (IS_LONG_INT_CONE(cone)) {
        Cone<long>* C = GET_CONE<long>(cone);
        return C->isInhomogeneous() ? True : False;
    } else {
        Cone<mpz_class>* C = GET_CONE<mpz_class>(cone);
        return C->isInhomogeneous() ? True : False;
    }
    FUNC_END
}

/*
#! @Arguments cone
#! @Returns TODO
#! @Description
#! TODO
DeclareGlobalFunction("NmzEquations");
*/
Obj NmzEquations(Obj self, Obj cone)
{
    FUNC_BEGIN
    if (!IS_CONE(cone))
        ErrorQuit("<cone> must be a normaliz cone", 0, 0);
    if (IS_LONG_INT_CONE(cone)) {
        Cone<long>* C = GET_CONE<long>(cone);
        C->compute(ConeProperties(libnormaliz::ConeProperty::SupportHyperplanes));
        return NmzMatrixToGAP(C->getEquations());
    } else {
        Cone<mpz_class>* C = GET_CONE<mpz_class>(cone);
        C->compute(ConeProperties(libnormaliz::ConeProperty::SupportHyperplanes));
        return NmzMatrixToGAP(C->getEquations());
    }
    FUNC_END
}

/*
#! @Arguments cone
#! @Returns TODO
#! @Description
#! TODO
DeclareGlobalFunction("NmzCongruences");
*/
Obj NmzCongruences(Obj self, Obj cone)
{
    FUNC_BEGIN
    if (!IS_CONE(cone))
        ErrorQuit("<cone> must be a normaliz cone", 0, 0);
    if (IS_LONG_INT_CONE(cone)) {
        Cone<long>* C = GET_CONE<long>(cone);
        C->compute(ConeProperties(libnormaliz::ConeProperty::SupportHyperplanes));
        return NmzMatrixToGAP(C->getCongruences());
    } else {
        Cone<mpz_class>* C = GET_CONE<mpz_class>(cone);
        C->compute(ConeProperties(libnormaliz::ConeProperty::SupportHyperplanes));
        return NmzMatrixToGAP(C->getCongruences());
    }
    FUNC_END
}


typedef Obj (* GVarFuncType)(/*arguments*/);

#define GVAR_FUNC_TABLE_ENTRY(srcfile, name, nparam, params) \
  {#name, nparam, \
   params, \
   (GVarFuncType)name, \
   srcfile ":Func" #name }

// Table of functions to export
static StructGVarFunc GVarFuncs[] = {
    GVAR_FUNC_TABLE_ENTRY("normaliz.cc", NmzLongIntCone, 1, "list"),
    GVAR_FUNC_TABLE_ENTRY("normaliz.cc", NmzGMPCone, 1, "list"),

    GVAR_FUNC_TABLE_ENTRY("normaliz.cc", _NmzCompute, 2, "cone, props"),
    GVAR_FUNC_TABLE_ENTRY("normaliz.cc", NmzVerbose, 1, "value"),

    GVAR_FUNC_TABLE_ENTRY("normaliz.cc", NmzHasConeProperty, 2, "cone, prop"),
    GVAR_FUNC_TABLE_ENTRY("normaliz.cc", _NmzConeProperty, 2, "cone, prop"),
    GVAR_FUNC_TABLE_ENTRY("normaliz.cc", NmzKnownConeProperties, 1, "cone"),

    GVAR_FUNC_TABLE_ENTRY("normaliz.cc", NmzEmbeddingDimension, 1, "cone"),
    GVAR_FUNC_TABLE_ENTRY("normaliz.cc", _NmzBasisChange, 1, "cone"),
    GVAR_FUNC_TABLE_ENTRY("normaliz.cc", NmzRank, 1, "cone"),
    GVAR_FUNC_TABLE_ENTRY("normaliz.cc", NmzIsInhomogeneous, 1, "cone"),
    GVAR_FUNC_TABLE_ENTRY("normaliz.cc", NmzEquations, 1, "cone"),
    GVAR_FUNC_TABLE_ENTRY("normaliz.cc", NmzCongruences, 1, "cone"),

    { 0 } /* Finish with an empty entry */

};

/******************************************************************************
*F  InitKernel( <module> )  . . . . . . . . initialise kernel data structures
*/
static Int InitKernel( StructInitInfo *module )
{
    /* init filters and functions                                          */
    InitHdlrFuncsFromTable( GVarFuncs );

    InitCopyGVar( "TheTypeNormalizLongIntCone", &TheTypeNormalizLongIntCone );
    InitCopyGVar( "TheTypeNormalizGMPCone", &TheTypeNormalizGMPCone );

    InfoBags[T_NORMALIZ].name = "NormalizCone";
    InitMarkFuncBags(T_NORMALIZ, &MarkNoSubBags);
    InitFreeFuncBag(T_NORMALIZ, &NormalizFreeFunc);
    TypeObjFuncs[T_NORMALIZ] = &NormalizTypeFunc;

    /* return success                                                      */
    return 0;
}

/******************************************************************************
*F  InitLibrary( <module> ) . . . . . . .  initialise library data structures
*/
static Int InitLibrary( StructInitInfo *module )
{
    /* init filters and functions */
    InitGVarFuncsFromTable( GVarFuncs );

    /* return success                                                      */
    return 0;
}

/******************************************************************************
*F  InitInfopl()  . . . . . . . . . . . . . . . . . table of init functions
*/
static StructInitInfo module = {
#ifdef NORMALIZSTATIC
 /* type        = */ MODULE_STATIC,
#else
 /* type        = */ MODULE_DYNAMIC,
#endif
 /* name        = */ "normaliz",
 /* revision_c  = */ 0,
 /* revision_h  = */ 0,
 /* version     = */ 0,
 /* crc         = */ 0,
 /* initKernel  = */ InitKernel,
 /* initLibrary = */ InitLibrary,
 /* checkInit   = */ 0,
 /* preSave     = */ 0,
 /* postSave    = */ 0,
 /* postRestore = */ 0
};

#ifndef NORMALIZSTATIC
extern "C"
StructInitInfo * Init__Dynamic ( void )
{
  return &module;
}
#endif

extern "C"
StructInitInfo * Init__normaliz ( void )
{
    return &module;
}
