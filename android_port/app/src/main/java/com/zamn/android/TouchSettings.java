package com.zamn.android;

import android.content.Context;
import android.content.SharedPreferences;
import java.io.File;
import java.io.FileOutputStream;
import java.nio.charset.StandardCharsets;
import java.util.Locale;

/** Local-only touch layout. Game consumes the same ordered normalized positions. */
final class TouchSettings {
    static final String[] LABELS={"UP","DOWN","LEFT","RIGHT","CYCLE WEAPON","CHANGE ITEM","SHOOT","SELECT","START","PREV","NEXT","MAP"};
    static final String[] HUD_LABELS={"UP","DOWN","LEFT","RIGHT","CYCLE","CHANGE","SHOOT","SELECT","START","PREV","NEXT","MAP"};
    static final float[][] DEFAULTS={
        {.175f,.680f},{.175f,.830f},{.137f,.755f},{.213f,.755f},
        {.782f,.624f},{.733f,.847f},{.925f,.784f},{.455f,.924f},{.568f,.924f},
        {.100f,.423f},{.901f,.423f},{.920f,.602f}
    };
    final float[] x=new float[LABELS.length],y=new float[LABELS.length];
    final float[] sizes=new float[LABELS.length];
    float dpadSize=1f;
    boolean enabled=true,autoHide=true;
    float scale=1f;
    // 0: four-way cross with diagonals; 1: eight-way disc; 2: stick with thumb knob.
    int dpadStyle=1;
    boolean floating=false;
    float deadzone=.12f;
    private final SharedPreferences preferences;
    private final File config;
    TouchSettings(Context context){
        preferences=context.getSharedPreferences("touch_layout_r5",Context.MODE_PRIVATE);
        config=new File(context.getFilesDir(),"zamn_touch.cfg");
        dpadStyle=Math.max(0,Math.min(2,preferences.getInt("dpadStyle",1)));
        floating=preferences.getBoolean("floating",false);
        deadzone=clamp(preferences.getFloat("deadzone",.12f),.12f,.55f);
        enabled=preferences.getBoolean("enabled",true);
        autoHide=preferences.getBoolean("autohide",true);
        scale=clamp(preferences.getFloat("scale",1f),.70f,1.45f);
        dpadSize=clamp(preferences.getFloat("dpadSize",1f),.45f,1.7f);
        for(int i=0;i<x.length;i++){
            x[i]=clamp(preferences.getFloat("x"+i,DEFAULTS[i][0]),.035f,.965f);
            y[i]=clamp(preferences.getFloat("y"+i,DEFAULTS[i][1]),.16f,.965f);
            sizes[i]=clamp(preferences.getFloat("size"+i,1f),.4f,1.7f);
        }
        // Reduce R8's oversized four-point cross only if those coordinates are still defaults.
        // Do not override custom layouts users already positioned themselves.
        if(!preferences.getBoolean("r9_pad_migrated",false)){
            float[][] old={{.175f,.625f},{.175f,.885f},{.063f,.755f},{.288f,.755f}};
            boolean oldDefault=true;
            for(int i=0;i<4;i++)if(Math.abs(x[i]-old[i][0])>.003f||Math.abs(y[i]-old[i][1])>.003f)oldDefault=false;
            if(oldDefault)for(int i=0;i<4;i++){x[i]=DEFAULTS[i][0];y[i]=DEFAULTS[i][1];}
            preferences.edit().putBoolean("r9_pad_migrated",true).apply();
        }
        // R10: apply the new default on upgrades only when the player still has
        // the exact R9 stock pad settings. Preserve deliberately customized pads.
        if(!preferences.getBoolean("r10_pad_defaults_migrated",false)){
            if(dpadStyle==0 && Math.abs(deadzone-.25f)<.001f){
                dpadStyle=1;
                deadzone=.12f;
            }
            preferences.edit().putBoolean("r10_pad_defaults_migrated",true).apply();
        }
        // A first launch produces the native layout file too.
        write();
    }
    static float clamp(float v,float a,float b){return Math.max(a,Math.min(b,v));}
    void reset(){
        enabled=true;autoHide=true;scale=1f;dpadSize=1f;
        dpadStyle=1;floating=false;deadzone=.12f;
        for(int i=0;i<x.length;i++){x[i]=DEFAULTS[i][0];y[i]=DEFAULTS[i][1];sizes[i]=1f;}
        write();
    }
    void write(){
        SharedPreferences.Editor p=preferences.edit();
        p.putInt("dpadStyle",dpadStyle).putBoolean("floating",floating).putFloat("deadzone",deadzone);
        p.putBoolean("enabled",enabled).putBoolean("autohide",autoHide).putFloat("scale",scale).putFloat("dpadSize",dpadSize);
        StringBuilder s=new StringBuilder();
        s.append(String.format(Locale.US,"%d %d %.4f\n",enabled?1:0,autoHide?1:0,scale));
        for(int i=0;i<x.length;i++){
            x[i]=clamp(x[i],.035f,.965f);y[i]=clamp(y[i],.16f,.965f);
            p.putFloat("x"+i,x[i]);p.putFloat("y"+i,y[i]);p.putFloat("size"+i,sizes[i]);
            // Keep exactly 11 original coordinates before the R9 metadata.
            // A new MAP coordinate is stored after the existing R9 record.
            if(i<11)s.append(String.format(Locale.US,"%.5f %.5f\n",x[i],y[i]));
        }
        s.append(String.format(Locale.US,"%d %d %.3f\n",dpadStyle,floating?1:0,deadzone));
        s.append("R9");
        for(int i=0;i<11;i++)s.append(String.format(Locale.US," %.3f",clamp(sizes[i],.4f,1.7f)));
        s.append(String.format(Locale.US," %.3f\n",clamp(dpadSize,.45f,1.7f)));
        s.append(String.format(Locale.US,"MAP %.5f %.5f %.3f\n",x[11],y[11],clamp(sizes[11],.4f,1.7f)));
        p.apply();
        File tmp=new File(config.getParentFile(),"zamn_touch.cfg.tmp");
        try(FileOutputStream out=new FileOutputStream(tmp)){
            out.write(s.toString().getBytes(StandardCharsets.US_ASCII));out.getFD().sync();
            if(config.exists()&&!config.delete())throw new java.io.IOException("Can't replace HUD layout");
            if(!tmp.renameTo(config))throw new java.io.IOException("Can't commit HUD layout");
        }catch(Exception e){android.util.Log.e("ZAMN-R5","Cannot write touch settings",e);}
    }
}
