/*
 * experimental.cc
 *
 *  Created on: May 1, 2015
 *      Author: isovic
 */

#include "graphmap/graphmap.h"



struct ClusterAndIndices {
  Range query;
  Range ref;
  int32_t num_anchors = 0;
  int32_t coverage = 0;
  std::vector<int> lcskpp_indices;
};

int GraphMap::ExperimentalPostProcessRegionWithLCS_(ScoreRegistry* local_score, MappingData* mapping_data, const Index* index, const Index* indexsecondary_, const SingleSequence* read, const ProgramParameters* parameters) {
  LogSystem::GetInstance().VerboseLog(VERBOSE_LEVEL_MED_DEBUG | VERBOSE_LEVEL_HIGH_DEBUG, ((parameters->num_threads == 1) || ((int64_t) read->get_sequence_id()) == parameters->debug_read), FormatString("Entering function. [time: %.2f sec, RSS: %ld MB, peakRSS: %ld MB] current_readid = %ld, current_local_score = %ld\n", (((float) (clock())) / CLOCKS_PER_SEC), getCurrentRSS() / (1024 * 1024), getPeakRSS() / (1024 * 1024), read->get_sequence_id(), local_score->get_scores_id()), "PostProcessRegionWithLCS_");

  int lcskpp_length = 0;
  std::vector<int> lcskpp_indices;

  #ifndef RELEASE_VERSION
    if (parameters->verbose_level > 5 && read->get_sequence_id() == parameters->debug_read) {
      VerboseLocalScoresToFile(FormatString("temp/local_scores/scores-%ld.csv", local_score->get_scores_id()), read, local_score, NULL, 0, 0, false);
    }
  #endif

//  CalcLCSFromLocalScores2(&(local_score->get_registry_entries()), false, 0, 0, &lcskpp_length, &lcskpp_indices);
  CalcLCSFromLocalScoresCacheFriendly_(&(local_score->get_registry_entries()), false, 0, 0, &lcskpp_length, &lcskpp_indices);

  if (lcskpp_length == 0) {
    LogSystem::GetInstance().VerboseLog(VERBOSE_LEVEL_ALL_DEBUG, read->get_sequence_id() == parameters->debug_read, FormatString("Current local scores: %ld, lcskpp_length == 0 || best_score == NULL\n", local_score->get_scores_id()), "PostProcessRegionWithLCS_");
    return 1;
  }

//  std::vector<std::vector<int> > lcskpp_clusters;
//  std::vector<int> cluster;
  std::vector<ClusterAndIndices *> clusters;
  ClusterAndIndices *new_cluster = NULL;
  int64_t min_cluster_length = read->get_sequence_length() * 0.10f;
  int64_t min_covered_bases = read->get_sequence_length() * 0.02f;

  int64_t last_nonskipped_i = lcskpp_indices.size() + 1;
  for (int64_t i=(lcskpp_indices.size() - 1); i >= 0; i--) {
    /// Skip anchors which might be too erroneous.
    int64_t anchor_len_query = local_score->get_registry_entries().query_ends[lcskpp_indices.at(i)] - local_score->get_registry_entries().query_starts[lcskpp_indices.at(i)];
    int64_t anchor_len_ref = local_score->get_registry_entries().reference_ends[lcskpp_indices.at(i)] - local_score->get_registry_entries().reference_starts[lcskpp_indices.at(i)];
    float max_length = ((float) std::max(anchor_len_query, anchor_len_ref));
    float min_length = ((float) std::min(anchor_len_query, anchor_len_ref));
    if (min_length <= 0)
      continue;
    float anchor_error = max_length / min_length - 1.0f;
    if (anchor_error > parameters->error_rate)
      continue;

    if (last_nonskipped_i > lcskpp_indices.size()) {

//      cluster.push_back(lcskpp_indices.at(i));
    } else {
      int64_t distance_query = local_score->get_registry_entries().query_ends[lcskpp_indices.at(i)] - local_score->get_registry_entries().query_starts[lcskpp_indices.at(last_nonskipped_i)];
      int64_t distance_ref = local_score->get_registry_entries().reference_ends[lcskpp_indices.at(i)] - local_score->get_registry_entries().reference_starts[lcskpp_indices.at(last_nonskipped_i)];
      float max_length = ((float) std::max(distance_query, distance_ref));
      float min_length = ((float) std::min(distance_query, distance_ref));
      if ((min_length == 0 && max_length != 0) || (min_length > 0 && (max_length / min_length - 1.0f) > parameters->error_rate)) {
        if (new_cluster != NULL) {
          clusters.push_back(new_cluster);
          new_cluster = NULL;
        }
//        printf ("Tu sam 1! distance_query = %ld, distance_ref = %ld\n", distance_query, distance_ref);
//        fflush(stdout);
      }
    }
//      cluster.push_back(lcskpp_indices[i]);
    if (new_cluster == NULL) {
      new_cluster = new ClusterAndIndices;
      new_cluster->query.start = local_score->get_registry_entries().query_starts[lcskpp_indices.at(i)];
      new_cluster->ref.start = local_score->get_registry_entries().reference_starts[lcskpp_indices.at(i)];
    }
    new_cluster->query.end = local_score->get_registry_entries().query_ends[lcskpp_indices.at(i)];
    new_cluster->ref.end = local_score->get_registry_entries().reference_ends[lcskpp_indices.at(i)];
    new_cluster->num_anchors += 1;
    new_cluster->coverage += local_score->get_registry_entries().covered_bases_queries[lcskpp_indices.at(i)];
    new_cluster->lcskpp_indices.push_back(lcskpp_indices.at(i));

    last_nonskipped_i = i;

//    fprintf (fp, "%ld\t%ld\n", local_score->get_registry_entries().query_starts[lcskpp_indices.at(i)], local_score->get_registry_entries().reference_starts[lcskpp_indices.at(i)]);
//    fprintf (fp, "%ld\t%ld\n", local_score->get_registry_entries().query_ends[lcskpp_indices.at(i)], local_score->get_registry_entries().reference_ends[lcskpp_indices.at(i)]);
//    float distance1 = abs((float) ((local_score->get_registry_entries().reference_starts[lcskpp_indices.at(i)] - local_score->get_registry_entries().query_starts[lcskpp_indices.at(i)]) - l_median) * (sqrt(2.0f)) / 2.0f);
//    float distance2 = abs((float) ((local_score->get_registry_entries().reference_ends[lcskpp_indices.at(i)] - local_score->get_registry_entries().query_ends[lcskpp_indices.at(i)]) - l_median) * (sqrt(2.0f)) / 2.0f);
  }

  if (new_cluster != NULL) {
    clusters.push_back(new_cluster);
    new_cluster = NULL;
  }

  #ifndef RELEASE_VERSION
    if (parameters->verbose_level > 5 && read->get_sequence_id() == parameters->debug_read) {
      LogSystem::GetInstance().VerboseLog(VERBOSE_LEVEL_HIGH_DEBUG, read->get_sequence_id() == parameters->debug_read, FormatString("LCSk clusters:\n"), "ExperimentalPostProcessRegionWithLCS_");
      for (int64_t i=0; i<clusters.size(); i++) {
//        LogSystem::GetInstance().VerboseLog(VERBOSE_LEVEL_HIGH_DEBUG, read->get_sequence_id() == parameters->debug_read, FormatString("[%ld] num_anchors: %ld, length: %ld\n", i, lcskpp_clusters[i].size(),
//                                                                                                                                      local_score->get_registry_entries().query_ends[lcskpp_clusters[i][lcskpp_clusters[i].size()-1]] - local_score->get_registry_entries().query_starts[lcskpp_clusters[i][0]]), "[]");
        LogSystem::GetInstance().VerboseLog(VERBOSE_LEVEL_HIGH_DEBUG, read->get_sequence_id() == parameters->debug_read, FormatString("[%ld] num_anchors: %ld, length: %ld, coverage: %ld, query.start = %ld, query.end = %ld\n", i, clusters[i]->num_anchors,
                                                                                                                                      (clusters[i]->query.end - clusters[i]->query.start), clusters[i]->coverage, clusters[i]->query.start, clusters[i]->query.end), "[]");

//        for (int64_t j=0; j<lcskpp_clusters[i].size(); j++) {
//          coverage +=
//        }
      }
      LogSystem::GetInstance().VerboseLog(VERBOSE_LEVEL_HIGH_DEBUG, read->get_sequence_id() == parameters->debug_read, "\n", "[]");
    }
  #endif

  std::vector<int> lcskpp_indices_clusters;
  int64_t current_cluster = clusters.size() - 1;
  for (int64_t i=0; i<clusters.size(); i++) {
    int64_t cluster_length = clusters[i]->query.end - clusters[i]->query.start;
    int64_t covered_bases = clusters[i]->coverage;
    if (cluster_length >= min_cluster_length && covered_bases >= min_covered_bases) {
      lcskpp_indices_clusters.insert(lcskpp_indices_clusters.end(), clusters[i]->lcskpp_indices.begin(), clusters[i]->lcskpp_indices.end());
    }
  }

  lcskpp_indices.clear();
  lcskpp_indices = lcskpp_indices_clusters;

//  for (int64_t i=0; i<lcskpp_indices.size(); i++) {
//    if (current_cluster < 0)
//      break;
//    printf ("current_cluster = %ld, clusters[current_cluster]->query.start = %ld, clusters[current_cluster]->query.end = %ld\n", current_cluster, clusters[current_cluster]->query.start, clusters[current_cluster]->query.end);
//    fflush(stdout);
//
//    if (local_score->get_registry_entries().query_starts[lcskpp_indices.at(i)] >= clusters[current_cluster]->query.start  && local_score->get_registry_entries().query_ends[lcskpp_indices.at(i)] <= clusters[current_cluster]->query.end) {
//      lcskpp_indices_clusters.push_back(lcskpp_indices[i]);
//      printf ("\tAdded: local_score->get_registry_entries().query_starts[lcskpp_indices.at(i)] = %ld, local_score->get_registry_entries().query_ends[lcskpp_indices.at(i)] = %ld\n", local_score->get_registry_entries().query_starts[lcskpp_indices.at(i)], local_score->get_registry_entries().query_ends[lcskpp_indices.at(i)]);
//      fflush(stdout);
//    } else {
//      printf ("\tSkipped: local_score->get_registry_entries().query_starts[lcskpp_indices.at(i)] = %ld, local_score->get_registry_entries().query_ends[lcskpp_indices.at(i)] = %ld\n", local_score->get_registry_entries().query_starts[lcskpp_indices.at(i)], local_score->get_registry_entries().query_ends[lcskpp_indices.at(i)]);
//      fflush(stdout);
//      current_cluster -= 1;
//    }
//  }



//  // Find the L1 parameters (median line and the confidence intervals).
  float l_diff = read->get_sequence_length() * parameters->error_rate;
  float maximum_allowed_deviation = l_diff * sqrt(2.0f) / 2.0f;
  float sigma_L2 = 0.0f, confidence_L1 = 0.0f;
  int64_t k = 0, l = 0;
//  // Actuall L1 calculation.
  int ret_L1 = CalculateL1ParametersWithMaximumDeviation_(local_score, lcskpp_indices, maximum_allowed_deviation, &k, &l, &sigma_L2, &confidence_L1);
  // Sanity check.
  if (ret_L1) {
    LogSystem::GetInstance().VerboseLog(VERBOSE_LEVEL_ALL_DEBUG, read->get_sequence_id() == parameters->debug_read, FormatString("An error occured, L1 function returned with %ld!\n", ret_L1), "L1-PostProcessRegionWithLCS_");
    return 1;
  }
  float allowed_L1_deviation = 3.0f * confidence_L1;

  #ifndef RELEASE_VERSION
    if (parameters->verbose_level > 5 && read->get_sequence_id() == parameters->debug_read) {
      LogSystem::GetInstance().VerboseLog(VERBOSE_LEVEL_HIGH_DEBUG, read->get_sequence_id() == parameters->debug_read, FormatString("l_median = %ld\n", l), "PostProcessRegionWithLCS_-DoubleLCSk");
      LogSystem::GetInstance().VerboseLog(VERBOSE_LEVEL_HIGH_DEBUG, read->get_sequence_id() == parameters->debug_read, FormatString("allowed_L1_deviation = %f\n", allowed_L1_deviation), "PostProcessRegionWithLCS_-DoubleLCSk");
      VerboseLocalScoresToFile(FormatString("temp/local_scores/LCS-%ld.csv", local_score->get_scores_id()), read, local_score, &lcskpp_indices, 0, 0, false);
//      VerboseLocalScoresToFile(FormatString("temp/local_scores/LCSL1-%ld.csv", local_score->get_scores_id()), read, local_score, &lcskpp_indices, l, allowed_L1_deviation, true);
      VerboseLocalScoresToFile(FormatString("temp/local_scores/LCSL1-%ld.csv", local_score->get_scores_id()), read, local_score, &lcskpp_indices_clusters, 0, 0, false);
    }
  #endif
//
//  lcskpp_indices.clear();
//
//  // Call the LCSk again, only on the bricks within the L1 bounded window.
//  CalcLCSFromLocalScoresCacheFriendly_(&(local_score->get_registry_entries()), true, l, allowed_L1_deviation, &lcskpp_length, &lcskpp_indices);

  // Count the number of covered bases, and find the first and last element of the LCSk.
  int64_t indexfirst = -1;
  int64_t indexlast = -1;

  int64_t covered_bases = 0;
  int64_t covered_bases_query = 0, covered_bases_reference = 0;
  int64_t num_covering_kmers = 0;
  LogSystem::GetInstance().VerboseLog(VERBOSE_LEVEL_HIGH_DEBUG, read->get_sequence_id() == parameters->debug_read, FormatString("Counting the covered bases and finding the first and the last brick index.\n"), "PostProcessRegionWithLCS_-DoubleLCSk");
  for (uint64_t i = 0; i < lcskpp_indices.size(); i++) {
    covered_bases_query += local_score->get_registry_entries().covered_bases_queries[lcskpp_indices[i]];
    covered_bases_reference += local_score->get_registry_entries().covered_bases_references[lcskpp_indices[i]];
    num_covering_kmers += local_score->get_registry_entries().num_kmers[lcskpp_indices[i]];
  }
  covered_bases = std::max(covered_bases_query, covered_bases_reference);



  if (lcskpp_indices.size() > 0) {
//    indexfirst = lcskpp_indices.front();
//    indexlast = lcskpp_indices.back();
    indexfirst = lcskpp_indices.back();
    indexlast = lcskpp_indices.front();
  }

  // There are no valid graph paths! All scores were dismissed because of high deviation.
  // This is most likely a false positive.
  if (indexfirst == -1 || indexlast == -1) {
    LogSystem::GetInstance().VerboseLog(VERBOSE_LEVEL_ALL_DEBUG, read->get_sequence_id() == parameters->debug_read, FormatString("An error occured, indexfirst = %ld, indexlast = %ld\n", indexfirst, indexlast), "L1-PostProcessRegionWithLCS_");
    return 1;
  }

//  ret_L1 = CalculateL1ParametersWithMaximumDeviation_(local_score, lcskpp_indices, maximum_allowed_deviation, &k, &l, &sigma_L2, &confidence_L1);


  if (parameters->verbose_level > 5 && read->get_sequence_id() == parameters->debug_read) {
    LogSystem::GetInstance().VerboseLog(VERBOSE_LEVEL_HIGH_DEBUG, read->get_sequence_id() == parameters->debug_read, FormatString("After second LCS calculation (with L1 filtering):\n", local_score->get_scores_id()), "PostProcessRegionWithLCS_2_");
    for (int64_t i = 0; i < lcskpp_indices.size(); i++) {
      LogSystem::GetInstance().VerboseLog(VERBOSE_LEVEL_HIGH_DEBUG, read->get_sequence_id() == parameters->debug_read, FormatString("[%ld] %s\n", i, local_score->get_registry_entries().VerboseToString(lcskpp_indices[i]).c_str()), "[]");
    }
    LogSystem::GetInstance().VerboseLog(VERBOSE_LEVEL_HIGH_DEBUG, read->get_sequence_id() == parameters->debug_read, FormatString("Checking index indices.\n"), "PostProcessRegionWithLCS_-DoubleLCSk");

    #ifndef RELEASE_VERSION
      VerboseLocalScoresToFile(FormatString("temp/local_scores/double_LCS-%ld.csv", local_score->get_scores_id()), read, local_score, &lcskpp_indices, l, 3.0f * confidence_L1, true);
    #endif
  }

  InfoMapping mapping_info;
  mapping_info.lcs_length = lcskpp_length;
  mapping_info.cov_bases_query = covered_bases_query;
  mapping_info.cov_bases_ref = covered_bases_reference;
  mapping_info.cov_bases_max = covered_bases;
  mapping_info.query_coords.start = local_score->get_registry_entries().query_starts[indexlast];
  mapping_info.query_coords.end = local_score->get_registry_entries().query_ends[indexfirst];
  mapping_info.ref_coords.start = local_score->get_registry_entries().reference_starts[indexlast];
  mapping_info.ref_coords.end = local_score->get_registry_entries().reference_ends[indexfirst];
  mapping_info.num_covering_kmers = num_covering_kmers;
  mapping_info.deviation = confidence_L1;
  mapping_info.is_reverse = (local_score->get_region().reference_id >= index->get_num_sequences_forward());
  mapping_info.local_score_id = local_score->get_scores_id();

  for (int64_t i=0; i<clusters.size(); i++) {
    if (clusters[i]) {
      int64_t cluster_length = clusters[i]->query.end - clusters[i]->query.start;
      if (cluster_length >= min_cluster_length) {
        Cluster mapping_cluster;
        mapping_cluster.query = clusters[i]->query;
        mapping_cluster.ref = clusters[i]->ref;
        mapping_info.clusters.push_back(mapping_cluster);
      }
      delete clusters[i];
    }
  }



  InfoL1 l1_info;
  l1_info.l1_l = l;
  l1_info.l1_k = 1.0f;
  l1_info.l1_lmin = l - l_diff;
  l1_info.l1_lmax = l + l_diff;
  l1_info.l1_confidence_abs = confidence_L1;
  l1_info.l1_std = sigma_L2;
  l1_info.l1_rough_start = l1_info.l1_k * 0 + l1_info.l1_lmin;
  l1_info.l1_rough_end = l1_info.l1_k * read->get_sequence_length() + l1_info.l1_lmax;
  if (l1_info.l1_rough_start < index->get_reference_starting_pos()[local_score->get_region().reference_id])
    l1_info.l1_rough_start = index->get_reference_starting_pos()[local_score->get_region().reference_id];
  if (l1_info.l1_rough_end >= (index->get_reference_starting_pos()[local_score->get_region().reference_id] + index->get_reference_lengths()[local_score->get_region().reference_id]))
    l1_info.l1_rough_end = (index->get_reference_starting_pos()[local_score->get_region().reference_id] + index->get_reference_lengths()[local_score->get_region().reference_id]) - 1;

  CheckMinimumMappingConditions_(&mapping_info, &l1_info, index, read, parameters);

  PathGraphEntry *new_entry = new PathGraphEntry(index, read, parameters, (Region &) local_score->get_region(), &mapping_info, &l1_info);

  float ratio = new_entry->CalcDistanceRatio();
  float ratio_suppress = new_entry->CalcDistanceRatioSuppress();
//  if (ratio_suppress > (parameters->error_rate * 2)) {
//    LogSystem::GetInstance().VerboseLog(VERBOSE_LEVEL_ALL_DEBUG, read->get_sequence_id() == parameters->debug_read, FormatString("Called unmapped, because ratio suppress is too high. new_entry->ratio_suppress = %.2f, max = %.2f.", ratio_suppress, (parameters->error_rate * 2)), "L1-PostProcessRegionWithLCS_");
//    delete new_entry;
//    return 1;
//  }

  LogSystem::GetInstance().VerboseLog(VERBOSE_LEVEL_HIGH_DEBUG, read->get_sequence_id() == parameters->debug_read, "Adding new entry.\n", "L1-PostProcessRegionWithLCS_");
  LogSystem::GetInstance().VerboseLog(VERBOSE_LEVEL_HIGH_DEBUG, read->get_sequence_id() == parameters->debug_read, "\n", "[]");
  mapping_data->intermediate_mappings.push_back(new_entry);

  LogSystem::GetInstance().VerboseLog(VERBOSE_LEVEL_MED_DEBUG | VERBOSE_LEVEL_HIGH_DEBUG, ((parameters->num_threads == 1) || read->get_sequence_id() == parameters->debug_read), FormatString("Exiting function. [time: %.2f sec, RSS: %ld MB, peakRSS: %ld MB]\n", (((float) (clock())) / CLOCKS_PER_SEC), getCurrentRSS() / (1024 * 1024), getPeakRSS() / (1024 * 1024)), "PostProcessRegionWithLCS_");

  return 0;
}






int AnchoredAlignmentLinear(AlignmentFunctionType AlignmentFunctionNW, AlignmentFunctionType AlignmentFunctionSemiglobal, const SingleSequence *read, const Index *index, const ProgramParameters &parameters, const PathGraphEntry *best_path,
                           int64_t *ret_alignment_position_left_part, std::string *ret_cigar_left_part, int64_t *ret_AS_left_part, int64_t *ret_nonclipped_left_part,
                           int64_t *ret_alignment_position_right_part, std::string *ret_cigar_right_part, int64_t *ret_AS_right_part, int64_t *ret_nonclipped_right_part,
                           SeqOrientation *ret_orientation, int64_t *ret_reference_id, int64_t *ret_position_ambiguity,
                           int64_t *ret_eq_op, int64_t *ret_x_op, int64_t *ret_i_op, int64_t *ret_d_op, bool perform_reverse_complement) {

//  ErrorReporting::GetInstance().VerboseLog(VERBOSE_LEVEL_ALL_DEBUG, true, "More generic implementation of the alignment step.\n", "LocalRealignmentLinear");

  int64_t absolute_reference_id = best_path->get_region_data().reference_id;
  int64_t reference_id = best_path->get_region_data().reference_id;
  int64_t reference_start = index->get_reference_starting_pos()[absolute_reference_id];
  int64_t reference_length = index->get_reference_lengths()[absolute_reference_id];
  SeqOrientation orientation = (best_path->get_region_data().reference_id >= index->get_num_sequences_forward()) ? (kReverse) : (kForward);
  int64_t best_aligning_position = 0;

  int64_t alignment_position_start = 0, alignment_position_end = 0, edit_distance = 0;
  std::vector<unsigned char> alignment;
  int64_t clip_count_front = 0, clip_count_back = 0;

  if (best_path->get_mapping_data().clusters.size() > 0) {
      clip_count_front = best_path->get_mapping_data().clusters.front().query.start;
      clip_count_back = read->get_sequence_length() - (best_path->get_mapping_data().clusters.back().query.end + parameters.k_graph);

      alignment_position_start = best_path->get_mapping_data().clusters.front().ref.start; // - clip_count_front;
      alignment_position_end = best_path->get_mapping_data().clusters.back().ref.end + parameters.k_graph - 1; // + clip_count_back;

      std::vector<unsigned char> insertions_front(clip_count_front, EDLIB_I);
      alignment.insert(alignment.begin(), insertions_front.begin(), insertions_front.end());
  }

  for (int64_t i=0; i<best_path->get_mapping_data().clusters.size(); i++) {
    /// Align the anchor.
    int64_t query_start = best_path->get_mapping_data().clusters[i].query.start;
    int64_t query_end = best_path->get_mapping_data().clusters[i].query.end + parameters.k_graph;
    int64_t ref_start = best_path->get_mapping_data().clusters[i].ref.start;
    int64_t ref_end = best_path->get_mapping_data().clusters[i].ref.end + parameters.k_graph;

    int64_t anchor_alignment_position_start = 0, anchor_alignment_position_end = 0, anchor_edit_distance = 0;
    std::vector<unsigned char> anchor_alignment;
    int ret_code1 = AlignmentFunctionNW(read->get_data() + query_start, (query_end - query_start),
                                     (int8_t *) (index->get_data() + ref_start), (ref_end - ref_start),
                                     -1, parameters.match_score, -parameters.mismatch_penalty, -parameters.gap_open_penalty, -parameters.gap_extend_penalty,
                                     &anchor_alignment_position_start, &anchor_alignment_position_end,
                                     &anchor_edit_distance, anchor_alignment);

    if (ret_code1 != 0 || anchor_alignment.size() == 0) {
      return ret_code1;
    }

    edit_distance += anchor_edit_distance;

////    alignment_position_start += ref_start;
////    alignment_position_end += ref_start;
    alignment.insert(alignment.end(), anchor_alignment.begin(), anchor_alignment.end());
//    std::vector<unsigned char> insertions_back(read->get_sequence_length() - (query_end + 1), EDLIB_I);
//    alignment.insert(alignment.end(), insertions_back.begin(), insertions_back.end());
//    alignment_position_start = ref_start + anchor_alignment_position_start;
//    alignment_position_end = ref_start + anchor_alignment_position_end;
//
//    std::string alignment_as_string = "";
//    alignment_as_string = PrintAlignmentToString((const unsigned char *) (read->get_data() + query_start), (query_end - query_start - 1),
//                                                 (const unsigned char *) (index->get_data() + ref_start), (ref_end - ref_start - 1),
//                                                 (unsigned char *) &(anchor_alignment[0]), anchor_alignment.size(),
//                                                 (anchor_alignment_position_start), MYERS_MODE_NW);
//    printf ("Alignment:\n");
//    printf ("%s\n", alignment_as_string.c_str());
//    fflush(stdout);
//
//    break;

    /// Align in between the anchors.
    if ((i + 1) < best_path->get_mapping_data().clusters.size()) {
//      printf ("In between alignment:\n");

      int64_t next_query_start = best_path->get_mapping_data().clusters[i+1].query.start;
      int64_t next_query_end = best_path->get_mapping_data().clusters[i+1].query.end + parameters.k_graph;
      int64_t next_ref_start = best_path->get_mapping_data().clusters[i+1].ref.start;
      int64_t next_ref_end = best_path->get_mapping_data().clusters[i+1].ref.end + parameters.k_graph;

//      printf ("query_start = %ld\n", query_start);
//      printf ("query_end = %ld\n", query_end);
//      printf ("ref_start = %ld\n", ref_start);
//      printf ("ref_end = %ld\n", ref_end);
//      printf ("next_query_start = %ld\n", next_query_start);
//      printf ("next_query_end = %ld\n", next_query_end);
//      printf ("next_ref_start = %ld\n", next_ref_start);
//      printf ("next_ref_end = %ld\n", next_ref_end);

      int64_t between_alignment_position_start = 0, between_alignment_position_end = 0, between_anchor_edit_distance = 0;
      std::vector<unsigned char> between_anchor_alignment;
      int ret_code2 = AlignmentFunctionNW(read->get_data() + (query_end), (next_query_start - (query_end)),
                                       (int8_t *) (index->get_data() + ref_end), (next_ref_start - (ref_end)),
                                       -1, parameters.match_score, -parameters.mismatch_penalty, -parameters.gap_open_penalty, -parameters.gap_extend_penalty,
                                       &between_alignment_position_start, &between_alignment_position_end,
                                       &between_anchor_edit_distance, between_anchor_alignment);

      if (ret_code2 != 0 || between_anchor_alignment.size() == 0)
        return ret_code2;
      edit_distance += between_anchor_edit_distance;
      alignment.insert(alignment.end(), between_anchor_alignment.begin(), between_anchor_alignment.end());

//    std::string alignment_as_string = "";
//    alignment_as_string = PrintAlignmentToString((const unsigned char *) read->get_data() + (query_end), (next_query_start - (query_end + 1)),
//                                                 (const unsigned char *) (index->get_data() + ref_end), (next_ref_start - (ref_end + 1)),
//                                                 (unsigned char *) &(between_anchor_alignment[0]), between_anchor_alignment.size(),
//                                                 (between_alignment_position_start), MYERS_MODE_NW);
//    printf ("Alignment:\n");
//    printf ("%s\n", alignment_as_string.c_str());
//    fflush(stdout);

    }
  }

  if (clip_count_back > 0) {
    std::vector<unsigned char> insertions_back(clip_count_back, EDLIB_I);
    alignment.insert(alignment.end(), insertions_back.begin(), insertions_back.end());
  }



//  int64_t l1_reference_start = best_path->get_l1_data().l1_lmin;
//  int64_t l1_reference_end = best_path->get_l1_data().l1_k * read->get_sequence_length() + best_path->get_l1_data().l1_lmax;
//  if (l1_reference_start < reference_start)
//    l1_reference_start = reference_start;
//  if (l1_reference_end >= (reference_start + reference_length))
//    l1_reference_end = reference_start + reference_length - 1;
//  int64_t reference_data_length = l1_reference_end - l1_reference_start + 1;

//  int ret_code = AlignmentFunction(read->get_data(), read->get_sequence_length(),
//                                   (int8_t *) (index->get_data() + l1_reference_start), reference_data_length,
//                                   -1, parameters.match_score, -parameters.mismatch_penalty, -parameters.gap_open_penalty, -parameters.gap_extend_penalty,
//                                   &alignment_position_start, &alignment_position_end,
//                                   &edit_distance, alignment);
//  alignment_position_start += l1_reference_start;
//  alignment_position_end += l1_reference_start;


//  std::string alignment_as_string = "";
//  alignment_as_string = PrintAlignmentToString((const unsigned char *) (read->get_data()), read->get_sequence_length(),
//                                               (const unsigned char *) (index->get_data() + alignment_position_start), (alignment_position_end - alignment_position_start + 1),
//                                               (unsigned char *) &(alignment[0]), alignment.size(),
//                                               (0), MYERS_MODE_NW);
//  printf ("Alignment:\n");
//  printf ("%s\n", alignment_as_string.c_str());
//  fflush(stdout);

  ConvertInsertionsToClipping((unsigned char *) &(alignment[0]), alignment.size());
  *ret_cigar_left_part = AlignmentToCigar((unsigned char *) &(alignment[0]), alignment.size());
//  *ret_AS_left_part = RescoreAlignment((unsigned char *) &(alignment[0]), alignment.size(), parameters.match_score, parameters.mismatch_penalty, parameters.gap_open_penalty, parameters.gap_extend_penalty);
  *ret_cigar_right_part = "";



  if (parameters.verbose_level > 5 && ((int64_t) read->get_sequence_id()) == parameters.debug_read) {
    std::string alignment_as_string = "";
//    alignment_as_string = PrintAlignmentToString((const unsigned char *) read->get_data(), read->get_sequence_length(),
//                                                 (const unsigned char *) (index->get_data() + l1_reference_start), reference_data_length,
//                                                 (unsigned char *) &(alignment[0]), alignment.size(),
//                                                 (alignment_position_end - l1_reference_start), MYERS_MODE_HW);

    LogSystem::GetInstance().VerboseLog(VERBOSE_LEVEL_ALL_DEBUG, ((int64_t) read->get_sequence_id()) == parameters.debug_read,
                                             FormatString("alignment_position_start = %ld\nalignment_position_end = %ld\n",
                                                          alignment_position_start, alignment_position_end), "LocalRealignmentLinear");

//    ErrorReporting::GetInstance().VerboseLog(VERBOSE_LEVEL_ALL_DEBUG, ((int64_t) read->get_sequence_id()) == parameters.debug_read,
//                                             FormatString("Ref:   %s\nRead:  %s\nCIGAR: %s\nEdit distance: %ld\nAlignment:\n%s\n\n",
//                                                          GetSubstring((char *) (index->get_data() + alignment_position_start), (alignment_position_end - alignment_position_start + 1)).c_str(),
//                                                          GetSubstring((char *) read->get_data(), read->get_sequence_length()).c_str(),
//                                                          (*ret_cigar).c_str(), edit_distance, alignment_as_string.c_str()), "[]");
  }

  if (orientation == kForward) {
    index->RawPositionConverter(alignment_position_start, 0, NULL, &best_aligning_position, NULL);
  } else {
    index->RawPositionConverter(alignment_position_end, 0, NULL, &best_aligning_position, NULL);
    if (perform_reverse_complement == true) {
//      *ret_cigar_left_part = ReverseCigarString((*ret_cigar_left_part));
//      read->ReverseComplement();
      LogSystem::GetInstance().VerboseLog(VERBOSE_LEVEL_ALL, ((int64_t) read->get_sequence_id()) == parameters.debug_read,
                                               FormatString("ERROR! Tried to explicitly reverse complement a read! This functionality is no longer allowed!"), "LocalRealignmentLinear");
      exit(1);
    }
    reference_id -= index->get_num_sequences_forward();
  }

  *ret_alignment_position_left_part = best_aligning_position;
  *ret_alignment_position_right_part = 0;
  *ret_orientation = orientation;
  *ret_reference_id = reference_id;
  *ret_position_ambiguity = 0;

  if (CheckAlignmentSane(alignment, read, index, reference_id, best_aligning_position))
    return -1;

  CountAlignmentOperations(alignment, read->get_data(), index->get_data(), reference_id, alignment_position_start, orientation,
                           parameters.evalue_match, parameters.evalue_mismatch, parameters.evalue_gap_open, parameters.evalue_gap_extend,
                           ret_eq_op, ret_x_op, ret_i_op, ret_d_op, ret_AS_left_part, ret_nonclipped_left_part);

//  printf ("AS: %ld\n", *ret_AS_left_part);
//  fflush(stdout);

  alignment.clear();

  return ((int) edit_distance);
}

int AnchoredAlignment2(bool is_linear, AlignmentFunctionType AlignmentFunctionNW, AlignmentFunctionType AlignmentFunctionSemiglobal, const SingleSequence *read, const Index *index, const ProgramParameters &parameters, const PathGraphEntry *best_path,
                           int64_t *ret_alignment_position_left_part, std::string *ret_cigar_left_part, int64_t *ret_AS_left_part, int64_t *ret_nonclipped_left_part,
                           int64_t *ret_alignment_position_right_part, std::string *ret_cigar_right_part, int64_t *ret_AS_right_part, int64_t *ret_nonclipped_right_part,
                           SeqOrientation *ret_orientation, int64_t *ret_reference_id, int64_t *ret_position_ambiguity,
                           int64_t *ret_eq_op, int64_t *ret_x_op, int64_t *ret_i_op, int64_t *ret_d_op, bool perform_reverse_complement) {

//  ErrorReporting::GetInstance().VerboseLog(VERBOSE_LEVEL_ALL_DEBUG, true, "More generic implementation of the alignment step.\n", "LocalRealignmentLinear");
  int8_t *ref_data = (int8_t *) index->get_data();
  int64_t region_length_joined = 0, start_offset = 0, position_of_ref_end = 0;

  if (is_linear == false) {
    if (best_path->get_region_data().is_split == false) {
      LogSystem::GetInstance().VerboseLog(VERBOSE_LEVEL_ALL_DEBUG, ((int64_t) read->get_sequence_id()) == parameters.debug_read, FormatString("Called the function for handling the circular part of the genome, but alignment is not split. best_path->region.is_split == false.\n\n"), "LocalRealignmentCircular");
      return -1;
    }

    ConcatenateSplitRegion(index, (Region &) best_path->get_region_data(), &ref_data, &region_length_joined, &start_offset, &position_of_ref_end);
  }

  int64_t absolute_reference_id = best_path->get_region_data().reference_id;
  int64_t reference_id = best_path->get_region_data().reference_id;
  int64_t reference_start = index->get_reference_starting_pos()[absolute_reference_id];
  int64_t reference_length = index->get_reference_lengths()[absolute_reference_id];
  SeqOrientation orientation = (best_path->get_region_data().reference_id >= index->get_num_sequences_forward()) ? (kReverse) : (kForward);
  int64_t best_aligning_position = 0;

  int64_t alignment_position_start = 0, alignment_position_end = 0, edit_distance = 0;
  std::vector<unsigned char> alignment;
  int64_t clip_count_front = 0, clip_count_back = 0;

  if (best_path->get_mapping_data().clusters.size() > 0) {
      clip_count_front = best_path->get_mapping_data().clusters.front().query.start;
      clip_count_back = read->get_sequence_length() - (best_path->get_mapping_data().clusters.back().query.end + parameters.k_graph);

      alignment_position_start = best_path->get_mapping_data().clusters.front().ref.start; // - clip_count_front;
      alignment_position_end = best_path->get_mapping_data().clusters.back().ref.end + parameters.k_graph - 1; // + clip_count_back;

      std::vector<unsigned char> insertions_front(clip_count_front, EDLIB_I);
      alignment.insert(alignment.begin(), insertions_front.begin(), insertions_front.end());
  }

  for (int64_t i=0; i<best_path->get_mapping_data().clusters.size(); i++) {
    /// Align the anchor.
    int64_t query_start = best_path->get_mapping_data().clusters[i].query.start;
    int64_t query_end = best_path->get_mapping_data().clusters[i].query.end + parameters.k_graph;
    int64_t ref_start = best_path->get_mapping_data().clusters[i].ref.start;
    int64_t ref_end = best_path->get_mapping_data().clusters[i].ref.end + parameters.k_graph;

    int64_t anchor_alignment_position_start = 0, anchor_alignment_position_end = 0, anchor_edit_distance = 0;
    std::vector<unsigned char> anchor_alignment;
    int ret_code1 = AlignmentFunctionNW(read->get_data() + query_start, (query_end - query_start),
                                     (int8_t *) (ref_data + ref_start), (ref_end - ref_start),
                                     -1, parameters.match_score, -parameters.mismatch_penalty, -parameters.gap_open_penalty, -parameters.gap_extend_penalty,
                                     &anchor_alignment_position_start, &anchor_alignment_position_end,
                                     &anchor_edit_distance, anchor_alignment);
    if (ret_code1 != 0 || anchor_alignment.size() == 0) {
      return ret_code1;
    }
    edit_distance += anchor_edit_distance;
    alignment.insert(alignment.end(), anchor_alignment.begin(), anchor_alignment.end());

    /// Align in between the anchors.
    if ((i + 1) < best_path->get_mapping_data().clusters.size()) {
      int64_t next_query_start = best_path->get_mapping_data().clusters[i+1].query.start;
      int64_t next_query_end = best_path->get_mapping_data().clusters[i+1].query.end + parameters.k_graph;
      int64_t next_ref_start = best_path->get_mapping_data().clusters[i+1].ref.start;
      int64_t next_ref_end = best_path->get_mapping_data().clusters[i+1].ref.end + parameters.k_graph;

      int64_t between_alignment_position_start = 0, between_alignment_position_end = 0, between_anchor_edit_distance = 0;
      std::vector<unsigned char> between_anchor_alignment;
      int ret_code2 = AlignmentFunctionNW(read->get_data() + (query_end), (next_query_start - (query_end)),
                                       (int8_t *) (ref_data + ref_end), (next_ref_start - (ref_end)),
                                       -1, parameters.match_score, -parameters.mismatch_penalty, -parameters.gap_open_penalty, -parameters.gap_extend_penalty,
                                       &between_alignment_position_start, &between_alignment_position_end,
                                       &between_anchor_edit_distance, between_anchor_alignment);

      if (ret_code2 != 0 || between_anchor_alignment.size() == 0)
        return ret_code2;
      edit_distance += between_anchor_edit_distance;
      alignment.insert(alignment.end(), between_anchor_alignment.begin(), between_anchor_alignment.end());
    }
  }

  if (clip_count_back > 0) {
    std::vector<unsigned char> insertions_back(clip_count_back, EDLIB_I);
    alignment.insert(alignment.end(), insertions_back.begin(), insertions_back.end());
  }

  if (parameters.verbose_level > 5 && ((int64_t) read->get_sequence_id()) == parameters.debug_read) {
    LogSystem::GetInstance().VerboseLog(VERBOSE_LEVEL_ALL_DEBUG, ((int64_t) read->get_sequence_id()) == parameters.debug_read,
                                        FormatString("alignment_position_start = %ld\nalignment_position_end = %ld\n",
                                        alignment_position_start, alignment_position_end), "LocalRealignmentLinear");
  }

  ConvertInsertionsToClipping((unsigned char *) &(alignment[0]), alignment.size());
  CountAlignmentOperations(alignment, read->get_data(), ref_data, reference_id, alignment_position_start, orientation,
                           parameters.evalue_match, parameters.evalue_mismatch, parameters.evalue_gap_open, parameters.evalue_gap_extend,
                           ret_eq_op, ret_x_op, ret_i_op, ret_d_op, ret_AS_left_part, ret_nonclipped_left_part);




  if (is_linear == true) {
    *ret_cigar_left_part = AlignmentToCigar((unsigned char *) &(alignment[0]), alignment.size());
    *ret_cigar_right_part = "";

    if (orientation == kForward) {
      index->RawPositionConverter(alignment_position_start, 0, NULL, &best_aligning_position, NULL);
    } else {
      index->RawPositionConverter(alignment_position_end, 0, NULL, &best_aligning_position, NULL);
      if (perform_reverse_complement == true) {
  //      *ret_cigar_left_part = ReverseCigarString((*ret_cigar_left_part));
  //      read->ReverseComplement();
        LogSystem::GetInstance().VerboseLog(VERBOSE_LEVEL_ALL, ((int64_t) read->get_sequence_id()) == parameters.debug_read,
                                                 FormatString("ERROR! Tried to explicitly reverse complement a read! This functionality is no longer allowed!"), "LocalRealignmentLinear");
        exit(1);
      }
      reference_id -= index->get_num_sequences_forward();
    }

    *ret_alignment_position_left_part = best_aligning_position;
    *ret_alignment_position_right_part = 0;
    *ret_orientation = orientation;
    *ret_reference_id = reference_id;
    *ret_position_ambiguity = 0;

    if (CheckAlignmentSane(alignment, read, index, reference_id, best_aligning_position))
      return -1;

  } else {
    *ret_AS_right_part = *ret_AS_left_part;
    *ret_nonclipped_right_part = *ret_nonclipped_left_part;



    int64_t best_aligning_position = 0;

    LogSystem::GetInstance().VerboseLog(VERBOSE_LEVEL_ALL_DEBUG, ((int64_t) read->get_sequence_id()) == parameters.debug_read, FormatString("best_aligning_position_start = %ld\n", alignment_position_start), "LocalRealignmentCircular");
    LogSystem::GetInstance().VerboseLog(VERBOSE_LEVEL_ALL_DEBUG, ((int64_t) read->get_sequence_id()) == parameters.debug_read, FormatString("best_aligning_position_end = %ld\n", alignment_position_end), "LocalRealignmentCircular");

    int64_t left_alignment_length = 0, left_alignment_start = 0, left_alignment_end = 0;
    int64_t right_alignment_length = 0, right_alignment_start = 0, right_alignment_end = 0;
    unsigned char *left_alignment = NULL;
    unsigned char *right_alignment = NULL;
    std::vector<unsigned char> alignment_right_part;
    if (ClipCircularAlignment(alignment_position_start, alignment_position_end, (unsigned char *) &(alignment[0]), alignment.size(),
                          (int64_t) (read->get_sequence_length()), (int64_t) (index->get_reference_starting_pos()[absolute_reference_id]),
                          (int64_t) (index->get_reference_lengths()[absolute_reference_id]),
                          start_offset, position_of_ref_end,
                          &left_alignment, &left_alignment_length, &left_alignment_start, &left_alignment_end,
                          &right_alignment, &right_alignment_length, &right_alignment_start, &right_alignment_end) != 0) {
      alignment.clear();
      alignment.assign(left_alignment, (left_alignment + left_alignment_length));
      if (left_alignment)
        free(left_alignment);

      alignment_right_part.clear();
      alignment_right_part.assign(right_alignment, (right_alignment + right_alignment_length));
      if (right_alignment)
        free(right_alignment);
    }

    int64_t best_aligning_position_left_part = 0;
    if (alignment.size() > 0) {
      *ret_cigar_left_part = AlignmentToCigar((unsigned char *) &(alignment[0]), alignment.size());
  //    *ret_AS_left_part = RescoreAlignment((unsigned char *) &(alignment_left_part[0]), alignment_left_part.size(), parameters.match_score, parameters.mismatch_penalty, parameters.gap_open_penalty, parameters.gap_extend_penalty);

      if (orientation == kForward) {
        index->RawPositionConverter(left_alignment_start, 0, NULL, &best_aligning_position_left_part, NULL);
      } else {
        index->RawPositionConverter(left_alignment_end, 0, NULL, &best_aligning_position_left_part, NULL);
        if (perform_reverse_complement == true) {
          *ret_cigar_left_part = ReverseCigarString((*ret_cigar_left_part));
        }
      }
    } else {
      *ret_cigar_left_part = "";
    }

    int64_t best_aligning_position_right_part = 0;
    if (alignment_right_part.size() > 0) {
      *ret_cigar_right_part = AlignmentToCigar((unsigned char *) &(alignment_right_part[0]), alignment_right_part.size());
  //    *ret_AS_right_part = RescoreAlignment((unsigned char *) &(alignment_right_part[0]), alignment_right_part.size(), parameters.match_score, parameters.mismatch_penalty, parameters.gap_open_penalty, parameters.gap_extend_penalty);

      if (orientation == kForward) {
        index->RawPositionConverter(right_alignment_start, 0, NULL, &best_aligning_position_right_part, NULL);
      } else {
        index->RawPositionConverter(right_alignment_end, 0, NULL, &best_aligning_position_right_part, NULL);
        if (perform_reverse_complement == true) {
          *ret_cigar_right_part = ReverseCigarString((*ret_cigar_right_part));
        }
      }
    } else {
      *ret_cigar_right_part = "";
    }

    *ret_alignment_position_left_part = best_aligning_position_left_part;
    *ret_alignment_position_right_part = best_aligning_position_right_part;
    *ret_orientation = orientation;
    *ret_reference_id = reference_id;
    *ret_position_ambiguity = 0;

    if (ref_data)
      delete[] ref_data;
  }

  alignment.clear();

  return ((int) edit_distance);
}

int AnchoredAlignment(bool is_linear, bool end_to_end, AlignmentFunctionType AlignmentFunctionNW, AlignmentFunctionType AlignmentFunctionSHW, const SingleSequence *read, const Index *index, const ProgramParameters &parameters, const PathGraphEntry *best_path,
                           int64_t *ret_alignment_position_left_part, std::string *ret_cigar_left_part, int64_t *ret_AS_left_part, int64_t *ret_nonclipped_left_part,
                           int64_t *ret_alignment_position_right_part, std::string *ret_cigar_right_part, int64_t *ret_AS_right_part, int64_t *ret_nonclipped_right_part,
                           SeqOrientation *ret_orientation, int64_t *ret_reference_id, int64_t *ret_position_ambiguity,
                           int64_t *ret_eq_op, int64_t *ret_x_op, int64_t *ret_i_op, int64_t *ret_d_op, bool perform_reverse_complement) {

  if (best_path->get_mapping_data().clusters.size() <= 0) {
    LogSystem::GetInstance().VerboseLog(VERBOSE_LEVEL_ALL_DEBUG, ((int64_t) read->get_sequence_id()) == parameters.debug_read, "No valid anchors exist!", "LocalRealignmentCircular");
    return -12;
  }

//  ErrorReporting::GetInstance().VerboseLog(VERBOSE_LEVEL_ALL_DEBUG, true, "More generic implementation of the alignment step.\n", "LocalRealignmentLinear");
  int8_t *ref_data = (int8_t *) index->get_data();
  int64_t region_length_joined = 0, start_offset = 0, position_of_ref_end = 0;

  int64_t absolute_reference_id = best_path->get_region_data().reference_id;
  int64_t reference_id = best_path->get_region_data().reference_id;
  int64_t reference_start = index->get_reference_starting_pos()[absolute_reference_id];
  int64_t reference_length = index->get_reference_lengths()[absolute_reference_id];
  SeqOrientation orientation = (best_path->get_region_data().reference_id >= index->get_num_sequences_forward()) ? (kReverse) : (kForward);


  if (is_linear == false) {
    if (best_path->get_region_data().is_split == false) {
      LogSystem::GetInstance().VerboseLog(VERBOSE_LEVEL_ALL_DEBUG, ((int64_t) read->get_sequence_id()) == parameters.debug_read, FormatString("Called the function for handling the circular part of the genome, but alignment is not split. best_path->region.is_split == false.\n\n"), "LocalRealignmentCircular");
      return -15;
    }

    ConcatenateSplitRegion(index, (Region &) best_path->get_region_data(), &ref_data, &region_length_joined, &start_offset, &position_of_ref_end);
    reference_start = 0;
    reference_length = region_length_joined;
  }




  int64_t edit_distance = 0;
  std::vector<unsigned char> alignment;

  int64_t clip_count_front = best_path->get_mapping_data().clusters.front().query.start;
  int64_t clip_count_back = read->get_sequence_length() - (best_path->get_mapping_data().clusters.back().query.end + parameters.k_graph);

  int64_t alignment_position_start = best_path->get_mapping_data().clusters.front().ref.start; // - clip_count_front;
  int64_t alignment_position_end = best_path->get_mapping_data().clusters.back().ref.end + parameters.k_graph - 1; // + clip_count_back;

  int64_t query_start = best_path->get_mapping_data().clusters.front().query.start; // - clip_count_front;
  int64_t query_end = best_path->get_mapping_data().clusters.back().query.end + parameters.k_graph - 1; // + clip_count_back;

//  int64_t leftover_left_len = query_start;
//  int64_t leftover_right_len = read->get_sequence_length() - (query_end + 1);

  if (clip_count_front > 0) {
    /// Check if we need to extend the alignment to the left boundary. Also, even if the user specified it, if we are to close to the boundary, just clip it.
    if (end_to_end == false || ((alignment_position_start - clip_count_front*2) < reference_start)) {
      std::vector<unsigned char> insertions_front(clip_count_front, EDLIB_I);
      alignment.insert(alignment.begin(), insertions_front.begin(), insertions_front.end());

    } else {
      /// Reversing the sequences to make the semiglobal alignment of the trailing and leading parts.
      int8_t *reversed_query_front = reverse_data(read->get_data(), clip_count_front);
      int8_t *reversed_ref_front = reverse_data(ref_data + (alignment_position_start - 1) - (clip_count_front*2 - 1), clip_count_front*2);

      int64_t leftover_left_start = 0, leftover_left_end = 0, leftover_left_edit_distance = 0;
      std::vector<unsigned char> leftover_left_alignment;
      int ret_code_right = AlignmentFunctionSHW(reversed_query_front, (clip_count_front),
                                       (int8_t *) (reversed_ref_front), (clip_count_front*2),
                                       -1, parameters.match_score, -parameters.mismatch_penalty, -parameters.gap_open_penalty, -parameters.gap_extend_penalty,
                                       &leftover_left_start, &leftover_left_end,
                                       &leftover_left_edit_distance, leftover_left_alignment);
      if (ret_code_right != 0) {
        // TODO: This is a nasty hack. EDlib used to crash when query and target are extremely small, e.g. query = "C" and target = "TC".
        // In this manner we just ignore the leading part, and clip it.
        std::vector<unsigned char> insertions_front(clip_count_front, EDLIB_I);
        alignment.insert(alignment.begin(), insertions_front.begin(), insertions_front.end());
//        return ret_code_right*1000;

      } else {
        if (leftover_left_alignment.size() == 0) {
          std::vector<unsigned char> insertions_front(clip_count_front, EDLIB_I);
          alignment.insert(alignment.begin(), insertions_front.begin(), insertions_front.end());
        } else {
          unsigned char *reversed_alignment = reverse_data(&(leftover_left_alignment[0]), leftover_left_alignment.size());
          alignment.insert(alignment.begin(), reversed_alignment, reversed_alignment + leftover_left_alignment.size());
          alignment_position_start -= leftover_left_end + 1;
          if (reversed_alignment)
            free(reversed_alignment);
        }

        if (reversed_query_front)
          free(reversed_query_front);
        if (reversed_ref_front)
          free(reversed_ref_front);
      }

    }
  }



  for (int64_t i=0; i<best_path->get_mapping_data().clusters.size(); i++) {
    /// Align the anchor.
    int64_t query_start = best_path->get_mapping_data().clusters[i].query.start;
    int64_t query_end = best_path->get_mapping_data().clusters[i].query.end + parameters.k_graph;
    int64_t ref_start = best_path->get_mapping_data().clusters[i].ref.start;
    int64_t ref_end = best_path->get_mapping_data().clusters[i].ref.end + parameters.k_graph;

    int64_t anchor_alignment_position_start = 0, anchor_alignment_position_end = 0, anchor_edit_distance = 0;
    std::vector<unsigned char> anchor_alignment;
    int ret_code1 = AlignmentFunctionNW(read->get_data() + query_start, (query_end - query_start),
                                     (int8_t *) (ref_data + ref_start), (ref_end - ref_start),
                                     -1, parameters.match_score, -parameters.mismatch_penalty, -parameters.gap_open_penalty, -parameters.gap_extend_penalty,
                                     &anchor_alignment_position_start, &anchor_alignment_position_end,
                                     &anchor_edit_distance, anchor_alignment);
    if (ret_code1 != 0 || anchor_alignment.size() == 0) {
      return ret_code1*2000;
    }
    edit_distance += anchor_edit_distance;
    alignment.insert(alignment.end(), anchor_alignment.begin(), anchor_alignment.end());

    /// Align in between the anchors.
    if ((i + 1) < best_path->get_mapping_data().clusters.size()) {
      int64_t next_query_start = best_path->get_mapping_data().clusters[i+1].query.start;
      int64_t next_query_end = best_path->get_mapping_data().clusters[i+1].query.end + parameters.k_graph;
      int64_t next_ref_start = best_path->get_mapping_data().clusters[i+1].ref.start;
      int64_t next_ref_end = best_path->get_mapping_data().clusters[i+1].ref.end + parameters.k_graph;

      int64_t between_alignment_position_start = 0, between_alignment_position_end = 0, between_anchor_edit_distance = 0;
      std::vector<unsigned char> between_anchor_alignment;
      int ret_code2 = AlignmentFunctionNW(read->get_data() + (query_end), (next_query_start - (query_end)),
                                       (int8_t *) (ref_data + ref_end), (next_ref_start - (ref_end)),
                                       -1, parameters.match_score, -parameters.mismatch_penalty, -parameters.gap_open_penalty, -parameters.gap_extend_penalty,
                                       &between_alignment_position_start, &between_alignment_position_end,
                                       &between_anchor_edit_distance, between_anchor_alignment);

      if (ret_code2 != 0 || between_anchor_alignment.size() == 0)
        return ret_code2*3000;
      edit_distance += between_anchor_edit_distance;
      alignment.insert(alignment.end(), between_anchor_alignment.begin(), between_anchor_alignment.end());
    }
  }



  if (clip_count_back > 0) {
    /// Handle the clipping at the end, or extend alignment to the end of the sequence.
    if (end_to_end == false || (alignment_position_end + 1 + clip_count_back * 2) >= (reference_start + reference_length)) {
        std::vector<unsigned char> insertions_back(clip_count_back, EDLIB_I);
        alignment.insert(alignment.end(), insertions_back.begin(), insertions_back.end());

    } else {
      int64_t leftover_right_start = 0, leftover_right_end = 0, leftover_right_edit_distance = 0;
      std::vector<unsigned char> leftover_right_alignment;
      int ret_code_right = AlignmentFunctionSHW(read->get_data() + query_end + 1, (clip_count_back),
                                       (int8_t *) (ref_data + alignment_position_end + 1), (clip_count_back*2),
                                       -1, parameters.match_score, -parameters.mismatch_penalty, -parameters.gap_open_penalty, -parameters.gap_extend_penalty,
                                       &leftover_right_start, &leftover_right_end,
                                       &leftover_right_edit_distance, leftover_right_alignment);
      if (ret_code_right != 0) {
//        PrintSubstring((char *) (read->get_data() + query_end + 1), (clip_count_back), stdout);
//        printf ("\n");
//        PrintSubstring((char *) (ref_data + alignment_position_end + 1), (clip_count_back*2), stdout);
//        printf ("\n");
//        printf ("clip_count_back = %ld\n", clip_count_back);
//        fflush(stdout);

        // TODO: This is a nasty hack. EDlib used to crash when query and target are extremely small, e.g. query = "C" and target = "TC".
        // In this manner we just ignore the trailing part, and clip it.
        std::vector<unsigned char> insertions_back(clip_count_back, EDLIB_I);
        alignment.insert(alignment.end(), insertions_back.begin(), insertions_back.end());

//        return ret_code_right*4001;
      } else {
        if (leftover_right_alignment.size() == 0) {
          std::vector<unsigned char> insertions_back(clip_count_back, EDLIB_I);
          alignment.insert(alignment.end(), insertions_back.begin(), insertions_back.end());
        } else {
          alignment.insert(alignment.end(), leftover_right_alignment.begin(), leftover_right_alignment.end());
          alignment_position_end += leftover_right_end + 1;
        }

      }
    }
  }



  if (parameters.verbose_level > 5 && ((int64_t) read->get_sequence_id()) == parameters.debug_read) {
    LogSystem::GetInstance().VerboseLog(VERBOSE_LEVEL_ALL_DEBUG, ((int64_t) read->get_sequence_id()) == parameters.debug_read,
                                        FormatString("alignment_position_start = %ld\nalignment_position_end = %ld\n",
                                        alignment_position_start, alignment_position_end), "LocalRealignmentLinear");
  }




//  int64_t reconstructed_length = CalculateReconstructedLength((unsigned char *) &(alignment[0]), alignment.size());
//  int64_t alignment_position_end2 = alignment_position_start + reconstructed_length - 1;
//  printf ("alignment_position_end2 = %ld\n", alignment_position_end2);
//  alignment_position_end = alignment_position_start + reconstructed_length - 1;



//  int64_t best_aligning_position1 = 0;
//  index->RawPositionConverter(alignment_position_end, 0, NULL, &best_aligning_position1, NULL);
//
//  unsigned char *temp = reverse_data(&(alignment[0]), alignment.size());
//  std::vector<unsigned char> temp1;
//  int8_t *reverse_read = read->GetReverseComplement();
//  temp1.assign(temp, temp + alignment.size());
//  CountAlignmentOperations(temp1, reverse_read, ref_data + index->get_reference_starting_pos()[0], reference_id, best_aligning_position1, orientation,
//                           parameters.evalue_match, parameters.evalue_mismatch, parameters.evalue_gap_open, parameters.evalue_gap_extend,
//                           ret_eq_op, ret_x_op, ret_i_op, ret_d_op, ret_AS_left_part, ret_nonclipped_left_part);
//
//  printf ("index->get_reference_starting_pos()[reference_id] = %ld\n", index->get_reference_starting_pos()[reference_id]);
//  printf ("best_aligning_position1 = %ld\n", best_aligning_position1);
////    PrintSubstring((char *) (ref_data + index->get_reference_starting_pos()[reference_id] + best_aligning_position), 100);
////    printf ("\n");
////    PrintSubstring((char *) read->get_data(), 100);
////    printf ("\n");
////    fflush(stdout);
//    std::string alignment_as_string = "";
//    alignment_as_string = PrintAlignmentToString((const unsigned char *) (reverse_read), read->get_sequence_length(),
//                                                 (const unsigned char *) (ref_data + index->get_reference_starting_pos()[0] + best_aligning_position1), (alignment_position_end - alignment_position_start + 1),
//                                                 (unsigned char *) &(temp1[0]), temp1.size(),
//                                                 (0), MYERS_MODE_NW);
////  alignment_as_string = PrintAlignmentToString((const unsigned char *) (read->get_data()), read->get_sequence_length(),
////                                               (const unsigned char *) (ref_data + alignment_position_start), (alignment_position_end - alignment_position_start + 1),
////                                               (unsigned char *) &(alignment[0]), alignment.size(),
////                                               (0), MYERS_MODE_NW);
//  printf ("Alignment:\n");
//  printf ("%s\n", alignment_as_string.c_str());
//  fflush(stdout);
//  if (temp)
//    free(temp);
//  if (reverse_read)
//    delete[] reverse_read;




  ConvertInsertionsToClipping((unsigned char *) &(alignment[0]), alignment.size());

  CountAlignmentOperations(alignment, read->get_data(), ref_data, reference_id, alignment_position_start, orientation,
                           parameters.evalue_match, parameters.evalue_mismatch, parameters.evalue_gap_open, parameters.evalue_gap_extend,
                           ret_eq_op, ret_x_op, ret_i_op, ret_d_op, ret_AS_left_part, ret_nonclipped_left_part);





  int64_t best_aligning_position = 0;

  if (is_linear == true) {
    *ret_cigar_left_part = AlignmentToCigar((unsigned char *) &(alignment[0]), alignment.size());
    *ret_cigar_right_part = "";

    if (orientation == kForward) {
      index->RawPositionConverter(alignment_position_start, 0, NULL, &best_aligning_position, NULL);
    } else {
      index->RawPositionConverter(alignment_position_end, 0, NULL, &best_aligning_position, NULL);
      if (perform_reverse_complement == true) {
  //      *ret_cigar_left_part = ReverseCigarString((*ret_cigar_left_part));
  //      read->ReverseComplement();
        LogSystem::GetInstance().VerboseLog(VERBOSE_LEVEL_ALL, ((int64_t) read->get_sequence_id()) == parameters.debug_read,
                                                 FormatString("ERROR! Tried to explicitly reverse complement a read! This functionality is no longer allowed!"), "LocalRealignmentLinear");
        exit(1);
      }
      reference_id -= index->get_num_sequences_forward();
    }




    *ret_alignment_position_left_part = best_aligning_position;
    *ret_alignment_position_right_part = 0;
    *ret_orientation = orientation;
    *ret_reference_id = reference_id;
    *ret_position_ambiguity = 0;

    if (CheckAlignmentSane(alignment, read, index, reference_id, best_aligning_position))
      return -123;




  } else {
    *ret_AS_right_part = *ret_AS_left_part;
    *ret_nonclipped_right_part = *ret_nonclipped_left_part;



    int64_t best_aligning_position = 0;

    LogSystem::GetInstance().VerboseLog(VERBOSE_LEVEL_ALL_DEBUG, ((int64_t) read->get_sequence_id()) == parameters.debug_read, FormatString("best_aligning_position_start = %ld\n", alignment_position_start), "LocalRealignmentCircular");
    LogSystem::GetInstance().VerboseLog(VERBOSE_LEVEL_ALL_DEBUG, ((int64_t) read->get_sequence_id()) == parameters.debug_read, FormatString("best_aligning_position_end = %ld\n", alignment_position_end), "LocalRealignmentCircular");

    int64_t left_alignment_length = 0, left_alignment_start = 0, left_alignment_end = 0;
    int64_t right_alignment_length = 0, right_alignment_start = 0, right_alignment_end = 0;
    unsigned char *left_alignment = NULL;
    unsigned char *right_alignment = NULL;
    std::vector<unsigned char> alignment_right_part;
    if (ClipCircularAlignment(alignment_position_start, alignment_position_end, (unsigned char *) &(alignment[0]), alignment.size(),
                          (int64_t) (read->get_sequence_length()), (int64_t) (index->get_reference_starting_pos()[absolute_reference_id]),
                          (int64_t) (index->get_reference_lengths()[absolute_reference_id]),
                          start_offset, position_of_ref_end,
                          &left_alignment, &left_alignment_length, &left_alignment_start, &left_alignment_end,
                          &right_alignment, &right_alignment_length, &right_alignment_start, &right_alignment_end) != 0) {
      alignment.clear();
      alignment.assign(left_alignment, (left_alignment + left_alignment_length));
      if (left_alignment)
        free(left_alignment);

      alignment_right_part.clear();
      alignment_right_part.assign(right_alignment, (right_alignment + right_alignment_length));
      if (right_alignment)
        free(right_alignment);
    }

    int64_t best_aligning_position_left_part = 0;
    if (alignment.size() > 0) {
      *ret_cigar_left_part = AlignmentToCigar((unsigned char *) &(alignment[0]), alignment.size());
  //    *ret_AS_left_part = RescoreAlignment((unsigned char *) &(alignment_left_part[0]), alignment_left_part.size(), parameters.match_score, parameters.mismatch_penalty, parameters.gap_open_penalty, parameters.gap_extend_penalty);

      if (orientation == kForward) {
        index->RawPositionConverter(left_alignment_start, 0, NULL, &best_aligning_position_left_part, NULL);
      } else {
        index->RawPositionConverter(left_alignment_end, 0, NULL, &best_aligning_position_left_part, NULL);
        if (perform_reverse_complement == true) {
          *ret_cigar_left_part = ReverseCigarString((*ret_cigar_left_part));
        }
      }
    } else {
      *ret_cigar_left_part = "";
    }

    int64_t best_aligning_position_right_part = 0;
    if (alignment_right_part.size() > 0) {
      *ret_cigar_right_part = AlignmentToCigar((unsigned char *) &(alignment_right_part[0]), alignment_right_part.size());
  //    *ret_AS_right_part = RescoreAlignment((unsigned char *) &(alignment_right_part[0]), alignment_right_part.size(), parameters.match_score, parameters.mismatch_penalty, parameters.gap_open_penalty, parameters.gap_extend_penalty);

      if (orientation == kForward) {
        index->RawPositionConverter(right_alignment_start, 0, NULL, &best_aligning_position_right_part, NULL);
      } else {
        index->RawPositionConverter(right_alignment_end, 0, NULL, &best_aligning_position_right_part, NULL);
        if (perform_reverse_complement == true) {
          *ret_cigar_right_part = ReverseCigarString((*ret_cigar_right_part));
        }
      }
    } else {
      *ret_cigar_right_part = "";
    }

    *ret_alignment_position_left_part = best_aligning_position_left_part;
    *ret_alignment_position_right_part = best_aligning_position_right_part;
    *ret_orientation = orientation;
    *ret_reference_id = reference_id;
    *ret_position_ambiguity = 0;

    if (ref_data)
      delete[] ref_data;
  }

  alignment.clear();

  return ((int) edit_distance);
}
