package com.zamn.android;
import android.content.Context;import android.graphics.Canvas;import android.view.MotionEvent;
public class TestR6{
static void require(boolean b,String what){if(!b)throw new AssertionError(what);}
static void touch(XboxMenuView m,float x,float y){m.onTouchEvent(new MotionEvent(0,x,y));m.onTouchEvent(new MotionEvent(1,x,y));}
public static void main(String[] args){
  XboxMenuView m=new XboxMenuView(new Context()); m.reset("LEADERBOARDS","");
  int[] value={0}, opened={0},back={0};
  m.addChoice("RULESET",new String[]{"NORMAL / SAVABLE","NORMAL / NO SAVE","HARDCORE / SAVABLE","HARDCORE / NO SAVE"},0,n->value[0]=n,()->opened[0]++);
  m.setBackAction(()->back[0]++);
  m.add("FETCH TOP TIMES",()->{});m.onDraw(new Canvas());
  // Logical 256x224 mapped into an 800x600 4:3 phone view; note top of row at y54.
  float sx=800f/256f,sy=600f/224f;
  touch(m,224*sx,63*sy);require(value[0]==1,"right arrow must increment");
  touch(m,32*sx,63*sy);require(value[0]==0,"left arrow must decrement");
  require(m.adjustSelected(-1)&&value[0]==3,"gamepad left must wrap");
  touch(m,123*sx,63*sy);require(opened[0]==1,"middle must open choices");
  touch(m,210*sx,202*sy);require(back[0]==1,"back touch accessible");
  m.reset("CHOOSE LEVEL","");for(int i=0;i<56;i++){final int x=i;m.add("LEVEL "+i,()->opened[0]=x);}m.focusEntry(53);m.onDraw(new Canvas());
  // Index 53 is visible at slot 3 because max scroll is 50.
  touch(m,100*sx,(54+3*22+10)*sy);require(opened[0]==53,"tapping level 54 in long list must work");
  // Six-item ONLINE screen: sixth BACK must be inside the viewport.
  m.reset("ONLINE","");int[] picked={-1};
  String[] names={"PUBLIC ROOMS","DIRECT CONNECT","SOLO PLAY","LEADERBOARDS","PROFILE","BACK"};
  for(int i=0;i<names.length;i++){final int ix=i;m.add(names[i],()->picked[0]=ix);}
  m.onDraw(new Canvas());
  touch(m,210*sx,196*sy);require(picked[0]==5,"ONLINE BACK tile must be visible and clickable");
  System.out.println("PASS: R6 arrows, full chooser, 56-row hit targets, back, six-tile ONLINE");
}}
