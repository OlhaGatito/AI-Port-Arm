#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "display_contract.h"

static void golden_640x480_screen_default(void)
{
    sf_display_contract c = sf_display_contract_make(640, 480, NULL);
    assert(c.policy == SF_ASPECT_SCREEN);
    assert(c.drawable_width == 640 && c.drawable_height == 480);
    assert(c.android_width == 640 && c.android_height == 480);
    assert(c.lock_android_size == 1);
    assert(c.identity == 1);
    assert(c.requested_policy_valid == 1);
}

static void screen_policy_follows_panel(void)
{
    sf_display_contract c = sf_display_contract_make(1024, 768, "screen");
    assert(c.policy == SF_ASPECT_SCREEN);
    assert(c.android_width == 1024 && c.android_height == 768);
    assert(c.identity == 1);
    assert(c.lock_android_size == 1);

    sf_display_rect source;
    assert(sf_display_screen_source_rect(1024, 768, &source) == 0);
    assert(source.x == 0 && source.y == 0);
    assert(source.width == 1024 && source.height == 768);
}

static void fill_policy_keeps_legacy_169_expand(void)
{
    sf_display_contract c = sf_display_contract_make(1024, 768, "fill");
    assert(c.policy == SF_ASPECT_FILL);
    assert(c.android_width == 1024 && c.android_height == 768);

    sf_display_rect source;
    assert(sf_display_fill_source_rect(1024, 768, &source) == 1);
    assert(source.x == 0 && source.y == 96);
    assert(source.width == 1024 && source.height == 576);
}

static void native_is_an_explicit_rollback(void)
{
    sf_display_contract c = sf_display_contract_make(640, 480, "native");
    assert(c.policy == SF_ASPECT_NATIVE);
    assert(c.drawable_width == 640 && c.drawable_height == 480);
    assert(c.android_width == 1280 && c.android_height == 720);
    assert(c.lock_android_size == 0);
    assert(c.identity == 0);
}

static void golden_1280x720_is_identity(void)
{
    sf_display_contract screen =
        sf_display_contract_make(1280, 720, "screen");
    sf_display_contract fill = sf_display_contract_make(1280, 720, "fill");
    sf_display_contract native =
        sf_display_contract_make(1280, 720, "native");
    assert(screen.android_width == 1280 && screen.android_height == 720);
    assert(fill.android_width == 1280 && fill.android_height == 720);
    assert(native.android_width == 1280 && native.android_height == 720);
    assert(screen.identity == 1 && fill.identity == 1 && native.identity == 1);
    assert(screen.lock_android_size == 0 && fill.lock_android_size == 0 &&
           native.lock_android_size == 0);
}

static void widescreen_screen_preserves_measured_identity(void)
{
    sf_display_contract c = sf_display_contract_make(1920, 1080, "screen");
    assert(c.android_width == 1920 && c.android_height == 1080);
    assert(c.identity == 1);
    assert(c.lock_android_size == 0);
}

static void invalid_policy_is_visible_and_safe(void)
{
    sf_display_contract c = sf_display_contract_make(640, 480, "stretch");
    assert(c.policy == SF_ASPECT_SCREEN);
    assert(c.android_width == 640 && c.android_height == 480);
    assert(c.requested_policy_valid == 0);
    assert(strcmp(sf_display_policy_name(c.policy), "screen") == 0);
}

static void invalid_drawable_uses_bounded_fallback(void)
{
    sf_display_contract c = sf_display_contract_make(0, -1, "screen");
    assert(c.drawable_width == 1280 && c.drawable_height == 720);
    assert(c.android_width == 1280 && c.android_height == 720);
    assert(c.identity == 1);
}

static void fill_source_geometry_is_centered_and_bounded(void)
{
    sf_display_rect source;
    assert(sf_display_fill_source_rect(640, 480, &source) == 1);
    assert(source.x == 0 && source.y == 60);
    assert(source.width == 640 && source.height == 360);

    assert(sf_display_fill_source_rect(1280, 720, &source) == 0);
    assert(source.x == 0 && source.y == 0);
    assert(source.width == 1280 && source.height == 720);

    assert(sf_display_fill_source_rect(1920, 1200, &source) == 1);
    assert(source.x == 0 && source.y == 60);
    assert(source.width == 1920 && source.height == 1080);

    assert(sf_display_fill_source_rect(2560, 1080, &source) == 1);
    assert(source.x == 320 && source.y == 0);
    assert(source.width == 1920 && source.height == 1080);

    assert(sf_display_fill_source_rect(0, 480, &source) == -1);
    assert(sf_display_fill_source_rect(640, 480, NULL) == -1);
}

static void screen_source_is_full_drawable(void)
{
    sf_display_rect source;
    assert(sf_display_screen_source_rect(1024, 768, &source) == 0);
    assert(source.x == 0 && source.y == 0);
    assert(source.width == 1024 && source.height == 768);
    assert(sf_display_screen_source_rect(0, 768, &source) == -1);
    assert(sf_display_screen_source_rect(1024, 768, NULL) == -1);
}

int main(void)
{
    golden_640x480_screen_default();
    screen_policy_follows_panel();
    fill_policy_keeps_legacy_169_expand();
    native_is_an_explicit_rollback();
    golden_1280x720_is_identity();
    widescreen_screen_preserves_measured_identity();
    invalid_policy_is_visible_and_safe();
    invalid_drawable_uses_bounded_fallback();
    fill_source_geometry_is_centered_and_bounded();
    screen_source_is_full_drawable();
    puts("display contract tests: OK");
    return 0;
}
