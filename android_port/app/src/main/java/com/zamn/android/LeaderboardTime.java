package com.zamn.android;

import java.util.Locale;

/** Xbox R49.9 frame timing: SNES frames to MM:SS.mmm, rounding to nearest millisecond. */
final class LeaderboardTime {
    private LeaderboardTime() {}
    static String fromFrames(long frames) {
        if (frames < 0) frames = 0;
        long ms = (frames * 1000L + 30L) / 60L;
        return String.format(Locale.US, "%02d:%02d.%03d",
            ms / 60000L, (ms / 1000L) % 60L, ms % 1000L);
    }
}
