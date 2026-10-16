#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
cp tests/android_r7_ui/com/zamn/android/TestR7.java "$T"/junk.java 2>/dev/null || :
cat > "$T/TestR9.java" <<'JAVA'
package com.zamn.android;
import android.content.Context;
import android.graphics.Canvas;
import android.view.MotionEvent;
import android.view.View;
import java.lang.reflect.Field;
public class TestR9 {
  static void ok(boolean b,String m){if(!b)throw new AssertionError(m);}
  static float px(float x){return x*View.TEST_WIDTH/485f;}
  static float py(float y){return y*View.TEST_HEIGHT/224f;}
  static int scroll(XboxMenuView v)throws Exception{Field f=XboxMenuView.class.getDeclaredField("scroll");f.setAccessible(true);return f.getInt(v);}
  public static void main(String[] args)throws Exception{
    XboxMenuView v=new XboxMenuView(new Context());v.reset("CHOOSE LEVEL","SWIPE");
    for(int i=0;i<56;i++){v.add("LEVEL " + i,()->{});}
    v.onDraw(new Canvas());
    v.onTouchEvent(new MotionEvent(MotionEvent.ACTION_DOWN,px(220),py(181)));
    v.onTouchEvent(new MotionEvent(MotionEvent.ACTION_MOVE,px(220),py(108)));
    ok(scroll(v)>0,"scroll must advance while finger is DOWN");
    v.onTouchEvent(new MotionEvent(MotionEvent.ACTION_UP,px(220),py(108)));
    v.reset("TOP TIMES","LEVEL 04");
    int[] swipes={0};v.setHorizontalSwipeAction(delta->swipes[0]+=delta);
    v.addScore(1,"NAME","01:05.000","2026-10-15");v.onDraw(new Canvas());
    v.onTouchEvent(new MotionEvent(MotionEvent.ACTION_DOWN,px(300),py(95)));
    v.onTouchEvent(new MotionEvent(MotionEvent.ACTION_UP,px(190),py(95)));
    ok(swipes[0]==1,"left swipe moves to next level");
    v.onTouchEvent(new MotionEvent(MotionEvent.ACTION_DOWN,px(180),py(95)));
    v.onTouchEvent(new MotionEvent(MotionEvent.ACTION_UP,px(290),py(95)));
    ok(swipes[0]==0,"right swipe moves to previous level");
    System.out.println("PASS R9: scroll while dragging; leaderboard left/right swipe level changes");
  }
}
JAVA
javac --release 8 -d "$T/classes" $(find tests/android_r7_ui/android -name '*.java') \
  android_port/app/src/main/java/com/zamn/android/XboxMenuView.java "$T/TestR9.java"
java -cp "$T/classes" com.zamn.android.TestR9
