#ifndef test_h
#define test_h

#define MM_PER_INCH 25.4

typedef enum
{
  param_none = 0,
  param_bool,
  param_int,
  param_fixed,
  param_string
}
parameter_type;

typedef enum
{
  opt_num_opts = 0,
  opt_mode_group,
  opt_mode,
  opt_depth,
  opt_hand_scanner,
  opt_three_pass,		/* 5 */
  opt_three_pass_order,
  opt_resolution,
  opt_special_group,
  opt_test_picture,
  opt_invert_endianess,
  opt_read_limit,
  opt_read_limit_size,
  opt_read_delay,
  opt_read_delay_duration,
  opt_read_status_code,
  opt_ppl_loss,
  opt_fuzzy_parameters,
  opt_non_blocking,
  opt_select_fd,
  opt_enable_test_options,
  opt_print_options,
  opt_geometry_group,
  opt_tl_x,
  opt_tl_y,
  opt_br_x,
  opt_br_y,
  opt_bool_group,
  opt_bool_soft_select_soft_detect,
  opt_bool_hard_select_soft_detect,
  opt_bool_hard_select,
  opt_bool_soft_detect,
  opt_bool_soft_select_soft_detect_emulated,
  opt_bool_soft_select_soft_detect_auto,
  opt_int_group,
  opt_int,
  opt_int_constraint_range,
  opt_int_constraint_word_list,
  opt_int_array,
  opt_int_array_constraint_range,
  opt_int_array_constraint_word_list,
  opt_fixed_group,
  opt_fixed,
  opt_fixed_constraint_range,
  opt_fixed_constraint_word_list,
  opt_string_group,
  opt_string,
  opt_string_constraint_string_list,
  opt_string_constraint_long_string_list,
  opt_button_group,
  opt_button,
  /* must come last: */
  num_options
}
test_opts;

typedef union
{
  SANE_Word w;
  SANE_Word *wa;		/* word array */
  SANE_String s;
}
Option_Value;


typedef struct Test_Device
{
  struct Test_Device *next;
  SANE_Device sane;
  SANE_Option_Descriptor opt[num_options];
  Option_Value val[num_options];
  SANE_Parameters params;
  SANE_String name;
  SANE_Int reader_pid;
  SANE_Int pipe;
  FILE *pipe_handle;
  SANE_Word pass;
  SANE_Word bytes_per_line;
  SANE_Word pixels_per_line;
  SANE_Word lines;
  SANE_Int bytes_total;
  SANE_Bool open;
  SANE_Bool scanning;
  SANE_Bool cancelled;
  SANE_Bool eof;
}
Test_Device;

#endif /* test_h */
