package com.zamn.android;
import android.content.Context;
import android.graphics.Canvas;
import android.view.View;
import android.view.MotionEvent;
import java.lang.reflect.Field;

public class TestR7 {
    static void require(boolean condition,String msg){if(!condition)throw new AssertionError(msg);}
    static void tap(XboxMenuView v,int x,int y){
        v.onTouchEvent(new MotionEvent(MotionEvent.ACTION_DOWN,x,y));
        v.onTouchEvent(new MotionEvent(MotionEvent.ACTION_UP,x,y));
    }
    static float px(int logical,int logicalWidth){return logical*View.TEST_WIDTH/(float)logicalWidth;}
    static float py(int logical){return logical*View.TEST_HEIGHT/224f;}
    static int width(XboxMenuView m)throws Exception{
        Field f=XboxMenuView.class.getDeclaredField("canvasWidth");f.setAccessible(true);return f.getInt(m);
    }
    public static void main(String[] args)throws Exception{
        require("00:50.583".equals(LeaderboardTime.fromFrames(3035)),"sub-minute time");
        require("01:50.217".equals(LeaderboardTime.fromFrames(6613)),"minute + seconds");
        require("02:16.750".equals(LeaderboardTime.fromFrames(8205)),"leaderboard time from screenshot");
        require("60:00.000".equals(LeaderboardTime.fromFrames(216000)),"60 minute time");
        XboxMenuView menu=new XboxMenuView(new Context());menu.reset("TOP TIMES","NORMAL / SAVABLE / LEVEL 04");
        menu.addScore(1,"SWAGGER TWOCRIBOUS","02:16.750","2026-10-09");
        menu.addScore(2,"SCOOT SWAGGER","03:02.225","2026-10-09");
        menu.addScore(3,"GOSHEN NOVA","04:07.983","2026-10-09");
        menu.addScore(4,"GOSHEN SCOOT","04:56.783","2026-10-09");
        menu.addScore(5,"NOVA SWAGGER","05:23.950","2026-10-09");
        int[] back={0}; menu.setBackAction(()->back[0]++);
        menu.add("BACK",()->back[0]++);
        menu.onDraw(new Canvas());
        int lw=width(menu);
        require(lw>=480&&lw<=490,"full-width renderer must match landscape aspect ratio: "+lw);
        tap(menu,Math.round(px(lw-50,lw)),Math.round(py(203)));
        require(back[0]==1,"fullscreen back hitbox");
        menu.reset("LEADERBOARDS","");
        int[] rules={0};
        menu.addChoice("RULESET",new String[]{"NORMAL / SAVABLE","NORMAL / NO SAVE","HARDCORE / SAVABLE","HARDCORE / NO SAVE"},0,idx->rules[0]=idx,()->{});
        menu.onDraw(new Canvas());
        tap(menu,Math.round(px(lw-39,lw)),Math.round(py(63)));
        require(rules[0]==1,"right arrow at fullscreen width");
        tap(menu,Math.round(px(36,lw)),Math.round(py(63)));
        require(rules[0]==0,"left arrow at fullscreen width");
        menu.reset("ONLINE","");
        int[] selected={-1};
        for(int i=0;i<6;i++){final int k=i;menu.add("TEST "+i,()->selected[0]=k);}
        menu.onDraw(new Canvas());
        tap(menu,Math.round(px(lw-46,lw)),Math.round(py(194)));
        require(selected[0]==5,"sixth Online tile at fullscreen width");
        View.TEST_WIDTH=1024;View.TEST_HEIGHT=768;
        menu.onDraw(new Canvas());
        require(width(menu)==299,"4:3 viewport must keep square pixels without stretch");
        System.out.println("PASS R7: mm:ss.mmm; phone fullscreen 1804x832; five scores; back; arrows; online 6; adaptive ratio");
    }
}
