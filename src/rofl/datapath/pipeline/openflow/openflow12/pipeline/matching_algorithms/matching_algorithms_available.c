/**
 * automatically generated by ../../../../../../../../../src/rofl/datapath/pipeline/openflow/openflow12/pipeline/matching_algorithms/matching_algorithms_available_c.sh loop
 * do not edit
 */

#include "matching_algorithms.h"
#include "matching_algorithms_available.h"

#include "loop/of12_loop_match.h"
extern of12_matching_algorithms_functions_t of12_matching_algorithm_loop_maf;
/* Main matching algorithm array */
of12_matching_algorithms_functions_t of12_matching_algorithms[of12_matching_algorithm_count];


/* Filling */
void __of12_generate_matching_algorithm_list(){
of12_matching_algorithms[0] = of12_matching_algorithm_loop_maf;
}
