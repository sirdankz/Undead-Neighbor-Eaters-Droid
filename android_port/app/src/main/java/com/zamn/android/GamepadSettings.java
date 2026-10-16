package com.zamn.android;

import android.content.Context;
import android.content.SharedPreferences;
import android.view.KeyEvent;
import java.io.File;
import java.io.FileOutputStream;
import java.nio.charset.StandardCharsets;

/** Local remaps for standard SDL GameController buttons/trigger axes; never netplay data. */
final class GamepadSettings {
    static final String[] ACTIONS={"CYCLE WEAPON","SHOOT","CHANGE ITEM","PREVIOUS WEAPON","NEXT WEAPON","START","SELECT","MOVE UP","MOVE DOWN","MOVE LEFT","MOVE RIGHT","MAP / RADAR"};
    // SDL_GameControllerButton enum, and 100/101 for analog LT/RT.
    // MAP defaults to R1 (button 10); original 11 action mappings remain unchanged.
    static final int[] DEFAULTS={0,2,1,100,101,6,4,11,12,13,14,10};
    static final String[] INPUTS={"A","B","X","Y","BACK","GUIDE","START","LEFT STICK","RIGHT STICK","LB","RB","D-PAD UP","D-PAD DOWN","D-PAD LEFT","D-PAD RIGHT"};
    final int[] bindings=DEFAULTS.clone();
    final Context context;
    GamepadSettings(Context context){this.context=context;
        SharedPreferences p=context.getSharedPreferences("gamepad_r8",Context.MODE_PRIVATE);
        for(int i=0;i<bindings.length;i++)bindings[i]=p.getInt("action"+i,DEFAULTS[i]);
        write();
    }
    static String describe(int value){if(value==100)return "LT";if(value==101)return "RT";
        return value>=0&&value<INPUTS.length?INPUTS[value]:"UNKNOWN";
    }
    static int androidToSdl(int key){
        switch(key){
            case KeyEvent.KEYCODE_BUTTON_A:return 0;
            case KeyEvent.KEYCODE_BUTTON_B:return 1;
            case KeyEvent.KEYCODE_BUTTON_X:return 2;
            case KeyEvent.KEYCODE_BUTTON_Y:return 3;
            case KeyEvent.KEYCODE_BUTTON_SELECT:return 4;
            case KeyEvent.KEYCODE_BUTTON_MODE:return 5;
            case KeyEvent.KEYCODE_BUTTON_START:return 6;
            case KeyEvent.KEYCODE_BUTTON_THUMBL:return 7;
            case KeyEvent.KEYCODE_BUTTON_THUMBR:return 8;
            case KeyEvent.KEYCODE_BUTTON_L1:return 9;
            case KeyEvent.KEYCODE_BUTTON_R1:return 10;
            case KeyEvent.KEYCODE_DPAD_UP:return 11;
            case KeyEvent.KEYCODE_DPAD_DOWN:return 12;
            case KeyEvent.KEYCODE_DPAD_LEFT:return 13;
            case KeyEvent.KEYCODE_DPAD_RIGHT:return 14;
            case KeyEvent.KEYCODE_BUTTON_L2:return 100;
            case KeyEvent.KEYCODE_BUTTON_R2:return 101;
            default:return -1;
        }
    }
    void bind(int action,int input){
        if(action<0||action>=bindings.length||input<0)return;
        // Avoid two actions competing for one physical button: swap bindings.
        for(int i=0;i<bindings.length;i++)if(i!=action&&bindings[i]==input)bindings[i]=bindings[action];
        bindings[action]=input;write();
    }
    void reset(){System.arraycopy(DEFAULTS,0,bindings,0,bindings.length);write();}
    void write(){
        SharedPreferences.Editor prefs=context.getSharedPreferences("gamepad_r8",Context.MODE_PRIVATE).edit();
        StringBuilder s=new StringBuilder("R8\n");
        for(int i=0;i<bindings.length;i++){prefs.putInt("action"+i,bindings[i]);s.append(bindings[i]).append('\n');}
        prefs.apply();
        File f=new File(context.getFilesDir(),"zamn_gamepad.cfg");
        try(FileOutputStream out=new FileOutputStream(f)){out.write(s.toString().getBytes(StandardCharsets.US_ASCII));}
        catch(Exception e){android.util.Log.e("ZAMN-R8","Failed to write gamepad settings",e);}
    }
}
