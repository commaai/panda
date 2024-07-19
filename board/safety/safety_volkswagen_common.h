#ifndef SAFETY_VOLKSWAGEN_COMMON_H
#define SAFETY_VOLKSWAGEN_COMMON_H

const uint16_t FLAG_VOLKSWAGEN_LONG_CONTROL = 1;

bool volkswagen_longitudinal = false;
bool volkswagen_set_button_prev = false;
bool volkswagen_resume_button_prev = false;
int volkswagen_change_torque_prev = 0;
int volkswagen_steer_frame_cnt = 0;

#endif
