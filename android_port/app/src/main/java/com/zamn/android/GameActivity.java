package com.zamn.android;

import android.content.Intent;
import org.libsdl.app.SDLActivity;

/** Native pause UI can open Android's existing control editor over gameplay. */
public class GameActivity extends SDLActivity {
    public void openPauseSettings(final int mode){
        runOnUiThread(()->{
            Intent i=new Intent(this,SetupActivity.class);
            i.putExtra("pause_settings",true);
            i.putExtra("pause_settings_mode",mode);
            startActivity(i);
        });
    }
}
