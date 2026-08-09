#include "visualization.h"

// Monkey drummer visualization

void init_monkey_system(Visualizer *vis) {
    vis->monkey_state.left_arm_angle = 0.0;
    vis->monkey_state.right_arm_angle = 0.0;
    vis->monkey_state.left_stick_height = -10.0;
    vis->monkey_state.right_stick_height = -10.0;
    vis->monkey_state.next_arm_left = TRUE;
    vis->monkey_state.last_left_hit_time = -10.0;
    vis->monkey_state.last_right_hit_time = -10.0;
    vis->monkey_state.left_hit_active = FALSE;
    vis->monkey_state.right_hit_active = FALSE;
    vis->monkey_state.head_bob_offset = 0.0;
    vis->monkey_state.body_bounce = 0.0;
    vis->monkey_state.mouth_open = 0.0;
    for (int i = 0; i < 3; i++) {
        vis->monkey_state.blink_timer[i] = i * 0.9;
        vis->monkey_state.eye_closed[i] = FALSE;
        vis->monkey_state.ear_wiggle[i] = 0.0;
    }
    vis->monkey_state.tail_sway = 0.0;
    vis->monkey_state.chest_scale = 1.0;
    vis->monkey_state.glow_intensity = 0.0;
    vis->monkey_state.last_beat_time = 0.0;
}

void update_monkey(Visualizer *vis, double dt) {
    // Bass frequencies drive the drum hits
    double bass_energy = 0.0;
    for (int i = 0; i < VIS_FREQUENCY_BARS/4; i++) {
        bass_energy += vis->frequency_bands[i];
    }
    bass_energy /= (VIS_FREQUENCY_BARS / 4);

    // High frequencies drive cymbal shimmer / screech
    double high_freq_energy = 0.0;
    for (int i = VIS_FREQUENCY_BARS/2; i < VIS_FREQUENCY_BARS; i++) {
        high_freq_energy += vis->frequency_bands[i];
    }
    high_freq_energy /= (VIS_FREQUENCY_BARS / 2);

    // Beat/onset detection - alternates which arm strikes
    if (vis->volume_level > 0.05 && bass_energy > 0.35 &&
        (vis->time_offset - vis->monkey_state.last_beat_time) > 0.18) {
        vis->monkey_state.last_beat_time = vis->time_offset;

        if (vis->monkey_state.next_arm_left) {
            vis->monkey_state.left_hit_active = TRUE;
            vis->monkey_state.last_left_hit_time = vis->time_offset;
            vis->monkey_state.left_stick_height = -50.0;
        } else {
            vis->monkey_state.right_hit_active = TRUE;
            vis->monkey_state.last_right_hit_time = vis->time_offset;
            vis->monkey_state.right_stick_height = -50.0;
        }
        vis->monkey_state.next_arm_left = !vis->monkey_state.next_arm_left;
    }

    // Sticks recoil back up to resting position after a strike
    double rest_height = -12.0;
    vis->monkey_state.left_stick_height += (rest_height - vis->monkey_state.left_stick_height) * 11.0 * dt;
    vis->monkey_state.right_stick_height += (rest_height - vis->monkey_state.right_stick_height) * 11.0 * dt;

    // Arms swing to follow the stick strike
    vis->monkey_state.left_arm_angle += (vis->monkey_state.left_stick_height * 0.7 - vis->monkey_state.left_arm_angle) * 14.0 * dt;
    vis->monkey_state.right_arm_angle += (vis->monkey_state.right_stick_height * 0.7 - vis->monkey_state.right_arm_angle) * 14.0 * dt;

    if ((vis->time_offset - vis->monkey_state.last_left_hit_time) > 0.4) vis->monkey_state.left_hit_active = FALSE;
    if ((vis->time_offset - vis->monkey_state.last_right_hit_time) > 0.4) vis->monkey_state.right_hit_active = FALSE;

    // Head bob to the beat
    double target_bob = 0.0;
    if (vis->volume_level > 0.05) {
        target_bob = sin(vis->time_offset * 3.0) * bass_energy * 12.0;
    }
    vis->monkey_state.head_bob_offset += (target_bob - vis->monkey_state.head_bob_offset) * 6.0 * dt;

    // Body bounce
    double target_bounce = 0.0;
    if (vis->volume_level > 0.05) {
        target_bounce = vis->volume_level * 10.0;
    }
    vis->monkey_state.body_bounce += (target_bounce - vis->monkey_state.body_bounce) * 5.0 * dt;

    // Mouth / screech reacts to high frequencies
    double target_mouth = high_freq_energy * 1.6;
    if (target_mouth > 1.0) target_mouth = 1.0;
    vis->monkey_state.mouth_open += (target_mouth - vis->monkey_state.mouth_open) * 9.0 * dt;
    if (vis->monkey_state.mouth_open < 0.0) vis->monkey_state.mouth_open = 0.0;
    if (vis->monkey_state.mouth_open > 1.0) vis->monkey_state.mouth_open = 1.0;

    // Eye blinking and ear wiggle - independent per monkey so they don't move in lockstep
    for (int i = 0; i < 3; i++) {
        vis->monkey_state.blink_timer[i] += dt;
        if (!vis->monkey_state.eye_closed[i] &&
            vis->monkey_state.blink_timer[i] > 3.0 + (rand() % 2000) / 1000.0) {
            vis->monkey_state.eye_closed[i] = TRUE;
            vis->monkey_state.blink_timer[i] = 0.0;
        } else if (vis->monkey_state.eye_closed[i] && vis->monkey_state.blink_timer[i] > 0.15) {
            vis->monkey_state.eye_closed[i] = FALSE;
            vis->monkey_state.blink_timer[i] = 0.0;
        }

        // Each monkey's ears wiggle on their own phase rather than all at once
        double target_ear = bass_energy * 8.0 * sin(vis->time_offset * 3.0 + i * 2.4);
        vis->monkey_state.ear_wiggle[i] += (target_ear - vis->monkey_state.ear_wiggle[i]) * 8.0 * dt;
    }

    // Tail sway
    vis->monkey_state.tail_sway = sin(vis->time_offset * 2.2) * 12.0 * vis->volume_level;

    // Chest breathing
    double target_chest = 1.0 + vis->volume_level * 0.15;
    vis->monkey_state.chest_scale += (target_chest - vis->monkey_state.chest_scale) * 7.0 * dt;

    // Glow intensity
    double target_glow = bass_energy * 0.6;
    vis->monkey_state.glow_intensity += (target_glow - vis->monkey_state.glow_intensity) * 10.0 * dt;
}

// Music notes burst upward off whichever drum was just struck
void draw_music_notes_monkey(Visualizer *vis, cairo_t *cr, double drum_x, double drum_y, double scale, double time_since_hit) {
    if (time_since_hit > 0.6) return;

    for (int i = 0; i < 6; i++) {
        double note_progress = time_since_hit + i * 0.07;
        if (note_progress < 0.0 || note_progress > 0.6) continue;

        double note_x = drum_x + sin(i * 2.1) * 25 * scale;
        double note_y = drum_y - note_progress * 140 * scale;

        double fade = 1.0 - (note_progress / 0.6);
        double hue = fmod(0.15 * i + vis->time_offset * 0.1, 1.0);
        double r, g, b;
        hsv_to_rgb(hue, 0.8, 1.0, &r, &g, &b);

        cairo_set_source_rgba(cr, r, g, b, fade);
        double note_scale = scale * 0.6;

        cairo_arc(cr, note_x, note_y, 6 * note_scale, 0, 2 * M_PI);
        cairo_fill(cr);

        cairo_set_line_width(cr, 2 * note_scale);
        cairo_move_to(cr, note_x + 5 * note_scale, note_y);
        cairo_line_to(cr, note_x + 5 * note_scale, note_y - 18 * note_scale);
        cairo_stroke(cr);

        cairo_move_to(cr, note_x + 5 * note_scale, note_y - 18 * note_scale);
        cairo_curve_to(cr,
            note_x + 14 * note_scale, note_y - 16 * note_scale,
            note_x + 16 * note_scale, note_y - 9 * note_scale,
            note_x + 11 * note_scale, note_y - 4 * note_scale);
        cairo_fill(cr);
    }
}

void draw_drum_hit_particles(Visualizer *vis, cairo_t *cr, double drum_x, double drum_y, double scale, double time_since_hit) {
    if (time_since_hit > 0.35) return;

    for (int i = 0; i < 12; i++) {
        double angle = (double)i / 12.0 * 2.0 * M_PI;
        double distance = time_since_hit * 160.0 * scale;
        double px = drum_x + cos(angle) * distance;
        double py = drum_y + sin(angle) * distance * 0.5 - time_since_hit * 40 * scale;

        double fade = 1.0 - (time_since_hit / 0.35);

        cairo_set_source_rgba(cr, 1.0, 0.85, 0.3, fade * 0.8);
        cairo_arc(cr, px, py, 3 * scale, 0, 2 * M_PI);
        cairo_fill(cr);
    }
}

static void draw_drum(cairo_t *cr, double x, double y, double radius, double r, double g, double b) {
    // Drum body (barrel)
    cairo_set_source_rgb(cr, r * 0.6, g * 0.6, b * 0.6);
    cairo_rectangle(cr, x - radius, y, radius * 2, radius * 0.9);
    cairo_fill(cr);

    // Drum skin (top)
    cairo_set_source_rgb(cr, 0.92, 0.88, 0.78);
    cairo_save(cr);
    cairo_translate(cr, x, y);
    cairo_scale(cr, 1.0, 0.45);
    cairo_arc(cr, 0, 0, radius, 0, 2 * M_PI);
    cairo_fill(cr);
    cairo_restore(cr);

    // Rim accent
    cairo_set_source_rgb(cr, r, g, b);
    cairo_save(cr);
    cairo_translate(cr, x, y);
    cairo_scale(cr, 1.0, 0.45);
    cairo_set_line_width(cr, 5.0);
    cairo_arc(cr, 0, 0, radius, 0, 2 * M_PI);
    cairo_stroke(cr);
    cairo_restore(cr);
}

// Draws one monkey + its pair of drums, tinted with the given fur/drum colors.
// Draw order is back-to-front: tail -> body -> legs (tucked, short) -> drums
// (drawn AFTER legs so the drum skin covers them, never legs-over-drums) -> arms -> head.
static void draw_single_monkey(Visualizer *vis, cairo_t *cr, int monkey_index, double cx, double cy, double scale,
                                double fur_r, double fur_g, double fur_b,
                                double drum1_r, double drum1_g, double drum1_b,
                                double drum2_r, double drum2_g, double drum2_b) {
    double face_r = fur_r + 0.35, face_g = fur_g + 0.43, face_b = fur_b + 0.42;
    if (face_r > 1.0) face_r = 1.0;
    if (face_g > 1.0) face_g = 1.0;
    if (face_b > 1.0) face_b = 1.0;

    // Tail with sway
    double sway = vis->monkey_state.tail_sway * scale;
    cairo_set_source_rgb(cr, fur_r * 0.8, fur_g * 0.8, fur_b * 0.8);
    cairo_set_line_width(cr, 10 * scale);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_move_to(cr, cx, cy + 30 * scale);
    cairo_curve_to(cr,
        cx + 40 * scale + sway, cy + 60 * scale,
        cx + 20 * scale + sway * 1.5, cy + 100 * scale,
        cx - 10 * scale + sway * 2, cy + 90 * scale);
    cairo_stroke(cr);

    // Body
    cairo_set_source_rgb(cr, fur_r, fur_g, fur_b);
    cairo_arc(cr, cx, cy, 85 * scale, 0, 2 * M_PI);
    cairo_fill(cr);

    // Chest with breathing
    cairo_set_source_rgb(cr, face_r, face_g, face_b);
    cairo_save(cr);
    cairo_translate(cr, cx, cy + 15 * scale);
    cairo_scale(cr, vis->monkey_state.chest_scale, vis->monkey_state.chest_scale);
    cairo_arc(cr, 0, 0, 45 * scale, 0, 2 * M_PI);
    cairo_fill(cr);
    cairo_restore(cr);

    // Legs, short and tucked in close to the body (well clear of the drums)
    cairo_set_source_rgb(cr, fur_r * 0.9, fur_g * 0.9, fur_b * 0.9);
    cairo_set_line_width(cr, 18 * scale);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_move_to(cr, cx - 15 * scale, cy + 65 * scale);
    cairo_line_to(cr, cx - 40 * scale, cy + 82 * scale);
    cairo_stroke(cr);
    cairo_move_to(cr, cx + 15 * scale, cy + 65 * scale);
    cairo_line_to(cr, cx + 40 * scale, cy + 82 * scale);
    cairo_stroke(cr);

    // Drums, drawn after the legs so the drum skin sits cleanly in front of them
    double left_drum_x = cx - 130 * scale;
    double right_drum_x = cx + 130 * scale;
    double drum_y = cy + 90 * scale;
    double drum_radius = 52 * scale;

    draw_drum(cr, left_drum_x, drum_y, drum_radius, drum1_r, drum1_g, drum1_b);
    draw_drum(cr, right_drum_x, drum_y, drum_radius, drum2_r, drum2_g, drum2_b);

    // LEFT ARM + STICK - shoulder stays fixed, elbow and tip lerp from a raised
    // rest pose straight to the left drum's exact center, so the stick always
    // lands dead-on regardless of how far into the strike it currently is.
    {
        double shoulder_x = cx - 60 * scale, shoulder_y = cy - 20 * scale;
        double raised_elbow_x = shoulder_x - 15 * scale, raised_elbow_y = shoulder_y - 20 * scale;
        double raised_tip_x = shoulder_x - 25 * scale, raised_tip_y = shoulder_y - 65 * scale;
        double struck_elbow_x = shoulder_x - 15 * scale, struck_elbow_y = shoulder_y + 45 * scale;

        double t = (-12.0 - vis->monkey_state.left_stick_height) / (-12.0 - -50.0);
        if (t < 0.0) t = 0.0;
        if (t > 1.0) t = 1.0;

        double elbow_x = raised_elbow_x + (struck_elbow_x - raised_elbow_x) * t;
        double elbow_y = raised_elbow_y + (struck_elbow_y - raised_elbow_y) * t;
        double tip_x = raised_tip_x + (left_drum_x - raised_tip_x) * t;
        double tip_y = raised_tip_y + (drum_y - raised_tip_y) * t;

        cairo_set_source_rgb(cr, fur_r, fur_g, fur_b);
        cairo_set_line_width(cr, 16 * scale);
        cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
        cairo_move_to(cr, shoulder_x, shoulder_y);
        cairo_line_to(cr, elbow_x, elbow_y);
        cairo_stroke(cr);

        cairo_set_source_rgb(cr, 0.75, 0.55, 0.3);
        cairo_set_line_width(cr, 6 * scale);
        cairo_move_to(cr, elbow_x, elbow_y);
        cairo_line_to(cr, tip_x, tip_y);
        cairo_stroke(cr);

        // Hand gripping the stick, roughly a third of the way down from the elbow
        double hand_x = elbow_x + (tip_x - elbow_x) * 0.35;
        double hand_y = elbow_y + (tip_y - elbow_y) * 0.35;
        cairo_set_source_rgb(cr, fur_r, fur_g, fur_b);
        cairo_arc(cr, hand_x, hand_y, 11 * scale, 0, 2 * M_PI);
        cairo_fill(cr);
    }

    // RIGHT ARM + STICK - mirror of the left arm, targeting the right drum's center
    {
        double shoulder_x = cx + 60 * scale, shoulder_y = cy - 20 * scale;
        double raised_elbow_x = shoulder_x + 15 * scale, raised_elbow_y = shoulder_y - 20 * scale;
        double raised_tip_x = shoulder_x + 25 * scale, raised_tip_y = shoulder_y - 65 * scale;
        double struck_elbow_x = shoulder_x + 15 * scale, struck_elbow_y = shoulder_y + 45 * scale;

        double t = (-12.0 - vis->monkey_state.right_stick_height) / (-12.0 - -50.0);
        if (t < 0.0) t = 0.0;
        if (t > 1.0) t = 1.0;

        double elbow_x = raised_elbow_x + (struck_elbow_x - raised_elbow_x) * t;
        double elbow_y = raised_elbow_y + (struck_elbow_y - raised_elbow_y) * t;
        double tip_x = raised_tip_x + (right_drum_x - raised_tip_x) * t;
        double tip_y = raised_tip_y + (drum_y - raised_tip_y) * t;

        cairo_set_source_rgb(cr, fur_r, fur_g, fur_b);
        cairo_set_line_width(cr, 16 * scale);
        cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
        cairo_move_to(cr, shoulder_x, shoulder_y);
        cairo_line_to(cr, elbow_x, elbow_y);
        cairo_stroke(cr);

        cairo_set_source_rgb(cr, 0.75, 0.55, 0.3);
        cairo_set_line_width(cr, 6 * scale);
        cairo_move_to(cr, elbow_x, elbow_y);
        cairo_line_to(cr, tip_x, tip_y);
        cairo_stroke(cr);

        // Hand gripping the stick, roughly a third of the way down from the elbow
        double hand_x = elbow_x + (tip_x - elbow_x) * 0.35;
        double hand_y = elbow_y + (tip_y - elbow_y) * 0.35;
        cairo_set_source_rgb(cr, fur_r, fur_g, fur_b);
        cairo_arc(cr, hand_x, hand_y, 11 * scale, 0, 2 * M_PI);
        cairo_fill(cr);
    }

    // Ears with wiggle (independent phase per monkey)
    double ear_wiggle = vis->monkey_state.ear_wiggle[monkey_index] * scale;
    double head_bob = vis->monkey_state.head_bob_offset * scale;
    cairo_set_source_rgb(cr, fur_r, fur_g, fur_b);
    cairo_arc(cr, cx - 70 * scale - ear_wiggle, cy - 90 * scale + head_bob, 22 * scale, 0, 2 * M_PI);
    cairo_fill(cr);
    cairo_arc(cr, cx + 70 * scale + ear_wiggle, cy - 90 * scale + head_bob, 22 * scale, 0, 2 * M_PI);
    cairo_fill(cr);
    cairo_set_source_rgb(cr, face_r, face_g, face_b);
    cairo_arc(cr, cx - 70 * scale - ear_wiggle, cy - 90 * scale + head_bob, 12 * scale, 0, 2 * M_PI);
    cairo_fill(cr);
    cairo_arc(cr, cx + 70 * scale + ear_wiggle, cy - 90 * scale + head_bob, 12 * scale, 0, 2 * M_PI);
    cairo_fill(cr);

    // Head with bob
    double head_x = cx;
    double head_y = cy - 95 * scale + head_bob;

    cairo_set_source_rgb(cr, fur_r, fur_g, fur_b);
    cairo_arc(cr, head_x, head_y, 60 * scale, 0, 2 * M_PI);
    cairo_fill(cr);

    // Face patch
    cairo_set_source_rgb(cr, face_r, face_g, face_b);
    cairo_arc(cr, head_x, head_y + 8 * scale, 42 * scale, 0, 2 * M_PI);
    cairo_fill(cr);

    // Eyes
    double eye_y = head_y - 5 * scale;
    if (vis->monkey_state.eye_closed[monkey_index]) {
        cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
        cairo_set_line_width(cr, 4 * scale);
        cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
        cairo_move_to(cr, head_x - 25 * scale, eye_y);
        cairo_line_to(cr, head_x - 10 * scale, eye_y);
        cairo_stroke(cr);
        cairo_move_to(cr, head_x + 10 * scale, eye_y);
        cairo_line_to(cr, head_x + 25 * scale, eye_y);
        cairo_stroke(cr);
    } else {
        cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
        cairo_arc(cr, head_x - 17 * scale, eye_y, 10 * scale, 0, 2 * M_PI);
        cairo_fill(cr);
        cairo_arc(cr, head_x + 17 * scale, eye_y, 10 * scale, 0, 2 * M_PI);
        cairo_fill(cr);
        cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
        cairo_arc(cr, head_x - 17 * scale, eye_y, 5 * scale, 0, 2 * M_PI);
        cairo_fill(cr);
        cairo_arc(cr, head_x + 17 * scale, eye_y, 5 * scale, 0, 2 * M_PI);
        cairo_fill(cr);
    }

    // Mouth (screech opens with high frequencies)
    double mouth_gap = vis->monkey_state.mouth_open * 20 * scale;
    cairo_set_source_rgba(cr, 0.2, 0.05, 0.05, 0.9);
    cairo_move_to(cr, head_x - 18 * scale, head_y + 30 * scale);
    cairo_curve_to(cr,
        head_x - 10 * scale, head_y + 30 * scale + mouth_gap,
        head_x + 10 * scale, head_y + 30 * scale + mouth_gap,
        head_x + 18 * scale, head_y + 30 * scale);
    cairo_curve_to(cr,
        head_x + 10 * scale, head_y + 25 * scale,
        head_x - 10 * scale, head_y + 25 * scale,
        head_x - 18 * scale, head_y + 30 * scale);
    cairo_close_path(cr);
    cairo_fill(cr);

    // Nostrils
    cairo_set_source_rgb(cr, fur_r * 0.7, fur_g * 0.7, fur_b * 0.7);
    cairo_arc(cr, head_x - 6 * scale, head_y + 15 * scale, 3 * scale, 0, 2 * M_PI);
    cairo_fill(cr);
    cairo_arc(cr, head_x + 6 * scale, head_y + 15 * scale, 3 * scale, 0, 2 * M_PI);
    cairo_fill(cr);

    // Drum reactions - notes and particles burst from whichever drum was hit
    double time_since_left = vis->time_offset - vis->monkey_state.last_left_hit_time;
    double time_since_right = vis->time_offset - vis->monkey_state.last_right_hit_time;

    draw_music_notes_monkey(vis, cr, left_drum_x, drum_y, scale, time_since_left);
    draw_music_notes_monkey(vis, cr, right_drum_x, drum_y, scale, time_since_right);
    draw_drum_hit_particles(vis, cr, left_drum_x, drum_y, scale, time_since_left);
    draw_drum_hit_particles(vis, cr, right_drum_x, drum_y, scale, time_since_right);
}

void draw_monkey(Visualizer *vis, cairo_t *cr) {
    // Three monkeys spread across the full width so the panel isn't mostly empty space.
    double scale = (vis->height * 0.9 / 400.0) * 0.56;
    double cy = vis->height * 0.58 + vis->monkey_state.body_bounce;

    double centers_x[3] = { vis->width * 0.18, vis->width * 0.5, vis->width * 0.82 };

    // Shared glow behind the whole trio
    if (vis->monkey_state.glow_intensity > 0.1) {
        cairo_pattern_t *glow = cairo_pattern_create_radial(
            vis->width / 2.0, cy, 80 * scale,
            vis->width / 2.0, cy, vis->width * 0.6);

        double hue = fmod(vis->time_offset * 0.2, 1.0);
        double r, g, b;
        hsv_to_rgb(hue, 0.7, 1.0, &r, &g, &b);

        cairo_pattern_add_color_stop_rgba(glow, 0, r, g, b, vis->monkey_state.glow_intensity * 0.3);
        cairo_pattern_add_color_stop_rgba(glow, 1, r, g, b, 0);

        cairo_set_source(cr, glow);
        cairo_rectangle(cr, 0, 0, vis->width, vis->height);
        cairo_fill(cr);
        cairo_pattern_destroy(glow);
    }

    // Slightly different fur tones per monkey, each with its own drum color pair
    draw_single_monkey(vis, cr, 0, centers_x[0], cy, scale,
        0.55, 0.35, 0.18,   // medium brown fur
        0.75, 0.15, 0.1,    // red drum
        0.1, 0.35, 0.75);   // blue drum

    draw_single_monkey(vis, cr, 1, centers_x[1], cy, scale,
        0.63, 0.29, 0.15,   // reddish-brown fur
        0.15, 0.55, 0.2,    // green drum
        0.45, 0.15, 0.55);  // purple drum

    draw_single_monkey(vis, cr, 2, centers_x[2], cy, scale,
        0.42, 0.30, 0.22,   // dark grayish-brown fur
        0.85, 0.45, 0.05,   // orange drum
        0.1, 0.55, 0.5);    // teal drum
}
