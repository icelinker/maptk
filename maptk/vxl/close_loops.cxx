/*ckwg +5
 * Copyright 2014 by Kitware, Inc. All Rights Reserved. Please refer to
 * KITWARE_LICENSE.TXT for licensing information, or contact General Counsel,
 * Kitware, Inc., 28 Corporate Drive, Clifton Park, NY 12065.
 */

/**
 * \file
 * \brief Implementation of \link maptk::algo::close_loops_bad_frames_only
 *        close_loops_bad_frames_only \endlink
 */

#include <maptk/vxl/close_loops.h>
#include <maptk/core/algo/estimate_homography.h>

#include <algorithm>

#include <boost/bind.hpp>
#include <boost/foreach.hpp>


namespace maptk
{

namespace vxl
{


/// Private implementation class
class close_loops::priv
{
public:

  priv()
  : long_term_closure_enabled_( true ),
    max_search_distance_( 10000 ),
    checkpoint_percent_overlap_( 0.60 )
  {
  }

  priv( const priv& other )
  : long_term_closure_enabled_( other.long_term_closure_enabled_ ),
    max_search_distance_( other.max_search_distance_ ),
    checkpoint_percent_overlap_( other.checkpoint_percent_overlap_ )
  {
  }

  ~priv()
  {
  }

  /// Is long term loop closure enabled?
  bool long_term_closure_enabled_;

  /// Maximum past search distance in terms of number of frames
  unsigned max_search_distance_;

  /// Term which controls when we make new loop closure checkpoints
  double checkpoint_percent_overlap_;
};


/// Default Constructor
close_loops
::close_loops()
: d_( new priv() )
{
}


/// Copy Constructor
close_loops
::close_loops( const close_loops& other )
: close_loops_bad_frames_only( other ),
  d_( new priv( *other.d_ ) )
{
}


/// Destructor
close_loops
::~close_loops()
{
}

/// Get this alg's \link maptk::config_block configuration block \endlink
config_block_sptr
close_loops
::get_configuration() const
{
  // get base config from base class
  config_block_sptr config = close_loops_bad_frames_only::get_configuration();

  // Sub-algorithm implementation name + sub_config block
  // - Homography estimator algorithm
  maptk::algo::estimate_homography::get_nested_algo_configuration
    ( "homography_estimator", config, h_estimator_ );

  // Loop closure parameters
  /*config->set_value("bf_detection_enabled", bf_detection_enabled_,
                    "Should bad frame detection be enabled? This option will attempt to "
                    "bridge the gap between frames which don't meet certain criteria "
                    "(percentage of feature points tracked) and will instead attempt "
                    "to match features on the current frame against past frames to "
                    "meet this criteria. This is useful when there can be bad frames.");*/

  return config;
}


/// Set this algo's properties via a config block
void
close_loops
::set_configuration( config_block_sptr in_config )
{
  // Starting with our generated config_block to ensure that assumed values are present
  // An alternative is to check for key presence before performing a get_value() call.
  config_block_sptr config = this->get_configuration();
  config->merge_config( in_config );

  // Setting nested algorithm instances via setter methods instead of directly
  // assigning to instance property.
  maptk::algo::estimate_homography_sptr hest;
  maptk::algo::estimate_homography::set_nested_algo_configuration
    ("homography_estimator", config, hest);
  h_estimator_ = hest;

  // Settings for bad frame detection
  //bf_detection_enabled_ = config->get_value<bool>("bf_detection_enabled");
}


bool
close_loops
::check_configuration( config_block_sptr config ) const
{
  return close_loops_bad_frames_only::check_configuration( config );
}


track_set_sptr
close_loops
::stitch( frame_id_t frame_number, track_set_sptr input ) const
{
  return close_loops_bad_frames_only::stitch( frame_number, input );
}


} // end namespace vxl

} // end namespace maptk
