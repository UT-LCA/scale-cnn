//=================================================================
//
//  -- AUTO-GENERATED --
//
//  Top-level for $name network
//
//=================================================================

#include "${name}_layers.h"

void $name() {

   // Feature map memories (UltraRAMs)
$fmap_declarations
   // Filter memories (Block RAMs)
$filter_declarations
   // Call each layer in sequence.
$layer_calls
}
