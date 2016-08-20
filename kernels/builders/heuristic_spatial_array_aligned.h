// ======================================================================== //
// Copyright 2009-2016 Intel Corporation                                    //
//                                                                          //
// Licensed under the Apache License, Version 2.0 (the "License");          //
// you may not use this file except in compliance with the License.         //
// You may obtain a copy of the License at                                  //
//                                                                          //
//     http://www.apache.org/licenses/LICENSE-2.0                           //
//                                                                          //
// Unless required by applicable law or agreed to in writing, software      //
// distributed under the License is distributed on an "AS IS" BASIS,        //
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. //
// See the License for the specific language governing permissions and      //
// limitations under the License.                                           //
// ======================================================================== //

#pragma once

#include "heuristic_binning.h"
#include "heuristic_spatial.h"

namespace embree
{
  namespace isa
  { 
#define ENABLE_SPATIAL_SPLITS 1
#define ENABLE_ARRAY_CHECKS 0
#define OVERLAP_THRESHOLD 0.2f
#define USE_SPATIAL_SPLIT_SAH_THRESHOLD 0.98f


// todo: PRIMITIVES_PER_LEAF

#define DBG_PRINT(x) 
//PRINT(x)

    /*! Performs standard object binning */
#if defined(__AVX512F__)
    template<typename SplitPrimitive, typename PrimRef, size_t OBJECT_BINS = 16, size_t SPATIAL_BINS = 16, size_t PRIMITIVES_PER_LEAF = 4>
#else
      template<typename SplitPrimitive, typename PrimRef, size_t OBJECT_BINS = 32, size_t SPATIAL_BINS = 16, size_t PRIMITIVES_PER_LEAF = 4>
#endif
      struct HeuristicArraySpatialSAH
      {
        typedef BinSplit<OBJECT_BINS> Split;
        typedef BinInfo<OBJECT_BINS,PrimRef> ObjectBinner;
        typedef SpatialBinInfo<SPATIAL_BINS,PrimRef> SpatialBinner;
        typedef extended_range<size_t> Set;
        typedef SpatialBinSplit<SPATIAL_BINS> SpatialSplit;

#if defined(__AVX512F__)
        static const size_t PARALLEL_THRESHOLD = 3*1024; 
        static const size_t PARALLEL_FIND_BLOCK_SIZE = 768;
        static const size_t PARALLEL_PARITION_BLOCK_SIZE = 128;
#else
        static const size_t PARALLEL_THRESHOLD = 3*1024;
        static const size_t PARALLEL_FIND_BLOCK_SIZE = 1024;
        static const size_t PARALLEL_PARITION_BLOCK_SIZE = 128;
#endif
        __forceinline HeuristicArraySpatialSAH ()
          : prims0(nullptr) {}
        
        /*! remember prim array */
        __forceinline HeuristicArraySpatialSAH (const SplitPrimitive& splitPrimitive, PrimRef* prims0)
          : prims0(prims0), splitPrimitive(splitPrimitive) {}


        /*! compute extended ranges */
        __forceinline void setExtentedRanges(const Set& set, Set& lset, Set& rset)
        {
          assert(set.ext_range_size() > 0);
          const size_t parent_size          = set.size();
          const size_t ext_range_size       = set.ext_range_size();
          const size_t left_size            = lset.size();
          const float left_factor           = (float)left_size / parent_size;
          const size_t left_ext_range_size  = min((size_t)(floorf(left_factor * ext_range_size)),ext_range_size);
          const size_t right_ext_range_size = ext_range_size - left_ext_range_size;
          lset.set_ext_range(lset.end() + left_ext_range_size);
          rset.set_ext_range(rset.end() + right_ext_range_size);

        }

        /*! move ranges */
        __noinline void moveExtentedRange(const Set& set, const Set& lset, const PrimInfo& left, Set& rset, PrimInfo& right)
        {
          const size_t left_ext_range_size = lset.ext_range_size();
          const size_t right_size = rset.size();

          // has the left child an extended range ?
          if (left_ext_range_size > 0)
          {
            // left extended range smaller than right range ?
            if (left_ext_range_size < right_size)
            {
              // only move a small part of of the beginning of the right range to the end
              for (size_t i=rset.begin();i<rset.begin()+left_ext_range_size;i++)
                prims0[i+right_size] = prims0[i];
            }
            else
            {
              // no overlap, move entire right range to new location, can be made fully parallel
              for (size_t i=rset.begin();i<rset.end();i++)
                prims0[i+left_ext_range_size] = prims0[i];
            }
            // update right range
            assert(rset.ext_end() + left_ext_range_size == set.ext_end());
            rset.move_right(left_ext_range_size);
            right.begin = rset.begin();
            right.end = rset.end();
          }
        }

        /*! finds the best split */
        const Split find(Set& set, PrimInfo& pinfo, const size_t logBlockSize)
        {
#if ENABLE_ARRAY_CHECKS == 1
          PrimRef* const source = prims0;
          for (size_t i=set.begin();i<set.end();i++)
            assert(subset(source[i].bounds(),pinfo.geomBounds));
#endif
          SplitInfo info;
          
          /* sequential or parallel */ 
          Split object_split = pinfo.size() < PARALLEL_THRESHOLD ? sequential_object_find(set,pinfo,logBlockSize,info) : parallel_object_find(set,pinfo,logBlockSize,info);
          
#if ENABLE_SPATIAL_SPLITS == 1
          if (unlikely(set.has_ext_range()) && set.size() > PRIMITIVES_PER_LEAF)
          {
            const BBox3fa overlap = intersect(info.leftBounds, info.rightBounds);
            if (safeArea(overlap) >= OVERLAP_THRESHOLD*safeArea(pinfo.geomBounds))
            {
              /* sequential or parallel */ 
              SpatialSplit spatial_split = pinfo.size() < PARALLEL_THRESHOLD ? sequential_spatial_find(set, pinfo, logBlockSize) : parallel_spatial_find(set, pinfo, logBlockSize);

              /* valid spatial split, better SAH and number of splits do not exceed extended range */
              if (spatial_split.sah/object_split.sah <= USE_SPATIAL_SPLIT_SAH_THRESHOLD &&
                  spatial_split.left + spatial_split.right - set.size() <= set.ext_range_size())
              {
                DBG_PRINT(object_split);
                DBG_PRINT(set.has_ext_range());
                DBG_PRINT(set);
                DBG_PRINT(spatial_split);
                DBG_PRINT(spatial_split.left + spatial_split.right - set.size());
          
                set = create_spatial_splits(set, spatial_split, spatial_split.mapping);
                // set new range in priminfo
                pinfo.begin = set.begin();
                pinfo.end   = set.end();
                // mark that we have a spatial split 
                object_split.lcount = (unsigned int)-1;
                object_split.sah = spatial_split.sah;
                object_split.dim = spatial_split.dim;
                object_split.pos = spatial_split.pos;
                object_split.mapping.ofs = spatial_split.mapping.ofs;
                object_split.mapping.scale = spatial_split.mapping.scale;
              }
            }
          }
#endif
          return object_split;

        }

        /*! finds the best object split */
        __noinline const Split sequential_object_find(const Set& set, const PrimInfo& pinfo, const size_t logBlockSize, SplitInfo &info)
        {
          PrimRef* const source = prims0;
          ObjectBinner binner(empty); // FIXME: this clear can be optimized away
          const BinMapping<OBJECT_BINS> mapping(pinfo);
          binner.bin(source,set.begin(),set.end(),mapping);
          Split s = binner.best(mapping,logBlockSize);
          s.lcount = (unsigned int)binner.getLeftCount(mapping,s);
          binner.getSplitInfo(mapping, s, info);
          return s;
        }

        /*! finds the best split */
        __noinline const Split parallel_object_find(Set& set, const PrimInfo& pinfo, const size_t logBlockSize, SplitInfo &info)
        {
          ObjectBinner binner(empty);
          const BinMapping<OBJECT_BINS> mapping(pinfo);
          const BinMapping<OBJECT_BINS>& _mapping = mapping; // CLANG 3.4 parser bug workaround
          binner = parallel_reduce(set.begin(),set.end(),PARALLEL_FIND_BLOCK_SIZE,binner,
                                   [&] (const range<size_t>& r) -> ObjectBinner { ObjectBinner binner(empty); binner.bin(prims0+r.begin(),r.size(),_mapping); return binner; },
                                   [&] (const ObjectBinner& b0, const ObjectBinner& b1) -> ObjectBinner { ObjectBinner r = b0; r.merge(b1,_mapping.size()); return r; });
          Split s = binner.best(mapping,logBlockSize);
          binner.getSplitInfo(mapping, s, info);
          return s;
        }


        /*! finds the best object split */
        __noinline const SpatialSplit sequential_spatial_find(Set& set, const PrimInfo& pinfo, const size_t logBlockSize)
        {
          PrimRef* const source = prims0;
          SpatialBinner binner(empty); 
          const SpatialBinMapping<SPATIAL_BINS> mapping(pinfo);
          binner.bin(splitPrimitive,source,set.begin(),set.end(),mapping);
          // todo: find best spatial split not exeeding the extended range
          return binner.best(pinfo,mapping,logBlockSize);
        }

        __noinline const SpatialSplit parallel_spatial_find(Set& set, const PrimInfo& pinfo, const size_t logBlockSize)
        {
          SpatialBinner binner(empty);
          const SpatialBinMapping<SPATIAL_BINS> mapping(pinfo);
          const SpatialBinMapping<SPATIAL_BINS>& _mapping = mapping; // CLANG 3.4 parser bug workaround
          binner = parallel_reduce(set.begin(),set.end(),PARALLEL_FIND_BLOCK_SIZE,binner,
                                   [&] (const range<size_t>& r) -> SpatialBinner { SpatialBinner binner(empty); binner.bin(splitPrimitive,prims0,r.begin(),r.end(),_mapping); return binner; },
                                   [&] (const SpatialBinner& b0, const SpatialBinner& b1) -> SpatialBinner { return SpatialBinner::reduce(b0,b1); });
          return binner.best(pinfo,mapping,logBlockSize);
        }


        /*! subdivides primitives based on a spatial split */
        __noinline Set create_spatial_splits(const Set& set, const SpatialSplit &split, const SpatialBinMapping<SPATIAL_BINS> &mapping)
        {
          assert(set.has_ext_range());
          const size_t max_ext_range_size = set.ext_range_size();
          const size_t ext_range_start = set.end();

          /* atomic counter for number of primref splits */
          std::atomic<size_t> ext_elements;
          ext_elements.store(0);
          
          const float fpos = split.mapping.pos(split.pos,split.dim);
          
          parallel_for( set.begin(), set.end(), [&](const range<size_t>& r) {
              for (size_t i=r.begin();i<r.end();i++)
              {
                const unsigned int splits = prims0[i].geomID() >> 24;

                if (likely(splits <= 1)) continue; //todo: does this ever happen ?

                int bin0 = split.mapping.bin(prims0[i].lower)[split.dim];
                int bin1 = split.mapping.bin(prims0[i].upper)[split.dim];
                if (unlikely(bin0 < split.pos && bin1 >= split.pos))
                {

                  //if (prims0[i].lower[split.dim] < fpos &&  prims0[i].upper[split.dim] > fpos)
                  {
                    assert(splits > 1);

                    PrimRef left,right;
                    splitPrimitive(prims0[i],split.dim,fpos,left,right);
                
                    // no empty splits
                    if (unlikely(left.bounds().empty() || right.bounds().empty())) continue;
                
#if ENABLE_ARRAY_CHECKS == 1
                    assert(left.lower.x <= left.upper.x);
                    assert(left.lower.y <= left.upper.y);
                    assert(left.lower.z <= left.upper.z);
                
                    assert(right.lower.x <= right.upper.x);
                    assert(right.lower.y <= right.upper.y);
                    assert(right.lower.z <= right.upper.z);
#endif
                    left.lower.a  = (left.lower.a & 0x00FFFFFF) | ((splits-1) << 24);
                    right.lower.a = (right.lower.a & 0x00FFFFFF) | ((splits-1) << 24);

                    const size_t ID = ext_elements.fetch_add(1);

                    /* break if the number of subdivided elements are greater than the maximal allowed size */
                    if (unlikely(ID >= max_ext_range_size)) break;
                    assert(ID <= max_ext_range_size);
                    prims0[i] = left;
                    prims0[ext_range_start+ID] = right;     
                  }
                }
              }
            });

          assert(ext_elements.load() <= max_ext_range_size);
          assert(max_ext_range_size >= ext_elements);     
          assert(set.end()+ext_elements.load()<=set.ext_end());
          return Set(set.begin(),set.end()+ext_elements.load(),set.ext_end());
        }
                        
        
        /*! array partitioning */
        void split(const Split& split, const PrimInfo& pinfo, const Set& set, PrimInfo& left, Set& lset, PrimInfo& right, Set& rset) 
        {
          /* valid split */
          if (unlikely(!split.valid())) {
            deterministic_order(set);
            return splitFallback(set,left,lset,right,rset);
          }

          if (unlikely(split.lcount == (unsigned int)-1))
          {
            /* spatial split */
            if (likely(pinfo.size() < PARALLEL_THRESHOLD)) 
              sequential_spatial_split(split,set,left,lset,right,rset);
            else
              parallel_spatial_split(split,set,left,lset,right,rset);
          }
          else
          {
            /* object split */
            if (likely(pinfo.size() < PARALLEL_THRESHOLD)) 
              sequential_object_split(split,set,left,lset,right,rset);
            else
              parallel_object_split(split,set,left,lset,right,rset);
          }

          /* if we have an extended range, set extended child ranges and move right split range */
          if (unlikely(set.has_ext_range())) 
          {
            setExtentedRanges(set,lset,rset);
            moveExtentedRange(set,lset,left,rset,right);
          }

          assert(lset.begin() == left.begin);
          assert(lset.end()   == left.end);
          assert(lset.size()  == left.size());
          assert(rset.begin() == right.begin);
          assert(rset.end()   == right.end);
          assert(rset.size()  == right.size());
        }

        /*! array partitioning */
        void sequential_object_split(const Split& split, const Set& set, PrimInfo& left, Set& lset, PrimInfo& right, Set& rset) 
        {
          PrimRef* const source = prims0;
          const size_t begin = set.begin();
          const size_t end   = set.end();
          CentGeomBBox3fa local_left(empty);
          CentGeomBBox3fa local_right(empty);
          const unsigned int splitPos = split.pos;
          const unsigned int splitDim = split.dim;
          const unsigned int splitDimMask = (unsigned int)1 << splitDim; 

#if defined(__AVX512F__)
          const vint16 vSplitPos(splitPos);
          const vbool16 vSplitMask( splitDimMask );
#else
          const vint4 vSplitPos(splitPos);
          const vbool4 vSplitMask( (int)splitDimMask );
#endif

          size_t center = serial_partitioning(source,
                                              begin,end,local_left,local_right,
                                              [&] (const PrimRef& ref) { 
#if defined(__AVX512F__)
                                                return split.mapping.bin_unsafe(ref,vSplitPos,vSplitMask);                                                 
#else
                                                return any(((vint4)split.mapping.bin_unsafe(center2(ref.bounds())) < vSplitPos) & vSplitMask); 
#endif
                                              },
                                              [] (CentGeomBBox3fa& pinfo,const PrimRef& ref) { pinfo.extend(ref.bounds()); });          
          
          //assert(center == begin + split.lcount);
          new (&left ) PrimInfo(begin,center,local_left.geomBounds,local_left.centBounds);
          new (&right) PrimInfo(center,end,local_right.geomBounds,local_right.centBounds);
          new (&lset) extended_range<size_t>(begin,center,center);
          new (&rset) extended_range<size_t>(center,end,end);

          assert(area(left.geomBounds) >= 0.0f);
          assert(area(right.geomBounds) >= 0.0f);

#if ENABLE_ARRAY_CHECKS == 1
          // verify that the left and right ranges are correct
          for (size_t i=lset.begin();i<lset.end();i++)
            assert(subset(source[i].bounds(),local_left.geomBounds));
          for (size_t i=rset.begin();i<rset.end();i++)
            assert(subset(source[i].bounds(),local_right.geomBounds));
#endif
        }


        /*! array partitioning */
        __noinline void sequential_spatial_split(const Split& split, const Set& set, PrimInfo& left, Set& lset, PrimInfo& right, Set& rset) 
        {
          assert(split.lcount == (unsigned int)-1);
          const size_t begin = set.begin();
          const size_t end   = set.end();
          CentGeomBBox3fa local_left(empty);
          CentGeomBBox3fa local_right(empty);
          const unsigned int splitPos = split.pos;
          const unsigned int splitDim = split.dim;
          const unsigned int splitDimMask = (unsigned int)1 << splitDim; 

          /* init spatial mapping */
          const SpatialBinMapping<SPATIAL_BINS> mapping(split.mapping.ofs,split.mapping.scale);
          const vint4 vSplitPos(splitPos);
          const vbool4 vSplitMask( (int)splitDimMask );

          size_t center = serial_partitioning(prims0,
                                              begin,end,local_left,local_right,
                                              [&] (const PrimRef& ref) {
                                                const Vec3fa c = ref.bounds().center();
                                                return any(((vint4)mapping.bin(c) < vSplitPos) & vSplitMask); 
                                              },
                                              [] (CentGeomBBox3fa& pinfo,const PrimRef& ref) { pinfo.extend(ref.bounds()); });          
          
          //assert(center == begin + split.lcount);
          new (&left ) PrimInfo(begin,center,local_left.geomBounds,local_left.centBounds);
          new (&right) PrimInfo(center,end,local_right.geomBounds,local_right.centBounds);
          new (&lset) extended_range<size_t>(begin,center,center);
          new (&rset) extended_range<size_t>(center,end,end);
          assert(area(left.geomBounds) >= 0.0f);
          assert(area(right.geomBounds) >= 0.0f);

          // verify that the left and right ranges are correct
#if ENABLE_ARRAY_CHECKS == 1
          for (size_t i=lset.begin();i<lset.end();i++)
            assert(subset(prims0[i].bounds(),local_left.geomBounds));
          for (size_t i=rset.begin();i<rset.end();i++)
            assert(subset(prims0[i].bounds(),local_right.geomBounds));
#endif
        }


        
        /*! array partitioning */
        __noinline void parallel_object_split(const Split& split, const Set& set, PrimInfo& left, Set& lset, PrimInfo& right, Set& rset)
        {
          const size_t begin = set.begin();
          const size_t end   = set.end();
          left.reset(); 
          right.reset();
          PrimInfo init; init.reset();
          const unsigned int splitPos = split.pos;
          const unsigned int splitDim = split.dim;
          const unsigned int splitDimMask = (unsigned int)1 << splitDim;

#if defined(__AVX512F__)
          const vint16 vSplitPos(splitPos);
          const vbool16 vSplitMask( (int)splitDimMask );
          auto isLeft = [&] (const PrimRef &ref) { return split.mapping.bin_unsafe(ref,vSplitPos,vSplitMask); };
#else
          const vint4 vSplitPos(splitPos);
          const vbool4 vSplitMask( (int)splitDimMask );
          auto isLeft = [&] (const PrimRef &ref) { return any(((vint4)split.mapping.bin_unsafe(center2(ref.bounds())) < vSplitPos) & vSplitMask); };

#endif
          const size_t mid = parallel_in_place_partitioning_static<PARALLEL_PARITION_BLOCK_SIZE,PrimRef,PrimInfo>(
            &prims0[begin],end-begin,init,left,right,isLeft,
            [] (PrimInfo &pinfo,const PrimRef &ref) { pinfo.add(ref.bounds()); },
            [] (PrimInfo &pinfo0,const PrimInfo &pinfo1) { pinfo0.merge(pinfo1); });
          
          const size_t center = begin+mid;
          left.begin  = begin;  left.end  = center; 
          right.begin = center; right.end = end;
          
          new (&lset) extended_range<size_t>(begin,center,center);
          new (&rset) extended_range<size_t>(center,end,end);

          assert(area(left.geomBounds) >= 0.0f);
          assert(area(right.geomBounds) >= 0.0f);

#if ENABLE_ARRAY_CHECKS == 1
          // verify that the left and right ranges are correct
          for (size_t i=lset.begin();i<lset.end();i++)
            assert(subset(prims0[i].bounds(),left.geomBounds));
          for (size_t i=rset.begin();i<rset.end();i++)
            assert(subset(prims0[i].bounds(),right.geomBounds));
#endif
        }

        /*! array partitioning */
        __noinline void parallel_spatial_split(const Split& split, const Set& set, PrimInfo& left, Set& lset, PrimInfo& right, Set& rset)
        {
          assert(split.lcount == (unsigned int)-1);
          const size_t begin = set.begin();
          const size_t end   = set.end();
          left.reset(); 
          right.reset();
          PrimInfo init; init.reset();
          const unsigned int splitPos = split.pos;
          const unsigned int splitDim = split.dim;
          const unsigned int splitDimMask = (unsigned int)1 << splitDim;

          /* init spatial mapping */
          const SpatialBinMapping<SPATIAL_BINS> mapping(split.mapping.ofs,split.mapping.scale);
          const vint4 vSplitPos(splitPos);
          const vbool4 vSplitMask( (int)splitDimMask );

          auto isLeft = [&] (const PrimRef &ref) { 
          const Vec3fa c = ref.bounds().center();
          return any(((vint4)mapping.bin(c) < vSplitPos) & vSplitMask); };

          const size_t mid = parallel_in_place_partitioning_static<PARALLEL_PARITION_BLOCK_SIZE,PrimRef,PrimInfo>(
            &prims0[begin],end-begin,init,left,right,isLeft,
            [] (PrimInfo &pinfo,const PrimRef &ref) { pinfo.add(ref.bounds()); },
            [] (PrimInfo &pinfo0,const PrimInfo &pinfo1) { pinfo0.merge(pinfo1); });
          
          const size_t center = begin+mid;
          left.begin  = begin;  left.end  = center; 
          right.begin = center; right.end = end;
          
          new (&lset) extended_range<size_t>(begin,center,center);
          new (&rset) extended_range<size_t>(center,end,end);

          assert(area(left.geomBounds) >= 0.0f);
          assert(area(right.geomBounds) >= 0.0f);

#if ENABLE_ARRAY_CHECKS == 1
          // verify that the left and right ranges are correct
          for (size_t i=lset.begin();i<lset.end();i++)
            assert(subset(prims0[i].bounds(),left.geomBounds));
          for (size_t i=rset.begin();i<rset.end();i++)
            assert(subset(prims0[i].bounds(),right.geomBounds));
#endif

        }


        void deterministic_order(const Set& set) 
        {
          /* required as parallel partition destroys original primitive order */
          std::sort(&prims0[set.begin()],&prims0[set.end()]);
        }

        void splitFallback(const Set& set, 
                           PrimInfo& linfo, Set& lset, 
                           PrimInfo& rinfo, Set& rset)
        {
          DBG_PRINT("FALLBACK");
          const size_t begin = set.begin();
          const size_t end   = set.end();
          const size_t center = (begin + end)/2;

          CentGeomBBox3fa left; left.reset();
          for (size_t i=begin; i<center; i++)
          {
            left.extend(prims0[i].bounds());
          }
          new (&linfo) PrimInfo(begin,center,left.geomBounds,left.centBounds);
          
          CentGeomBBox3fa right; right.reset();
          for (size_t i=center; i<end; i++)
          {
            right.extend(prims0[i].bounds());	
          }
          new (&rinfo) PrimInfo(center,end,right.geomBounds,right.centBounds);         
          new (&lset) extended_range<size_t>(begin,center,center);
          new (&rset) extended_range<size_t>(center,end,end);

          // if we have an extended range
          if (set.has_ext_range()) 
          {
            setExtentedRanges(set,lset,rset);
            moveExtentedRange(set,lset,linfo,rset,rinfo);              
          }
        }
        
      private:
        PrimRef* const prims0;
        const SplitPrimitive& splitPrimitive;

      };
  }
}
