// Header declaration for $lname
void $lname (
   data_t in_data[$input_height][$input_width][$input_chans_padded],
   data_t out_data[$output_height][$output_width][$output_chans],
   data_t l1_filter_data[$intermediate_chans][$filter_size][$filter_size][$input_chans],
   data_t l2_filter_data[$output_chans][$intermediate_chans]
);
