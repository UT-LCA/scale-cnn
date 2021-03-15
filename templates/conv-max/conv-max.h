// Header declaration for $lname
void $lname (
   data_t in_data[$input_height][$input_width][$input_chans_padded],
   data_t out_data[$output_height][$output_width][$output_chans],
   data_t filter_data[$output_chans][$filter_size][$filter_size][$input_chans],
   data_t adjustments[$output_chans][4]
);
