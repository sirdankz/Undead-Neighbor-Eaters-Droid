package com.zamn.android;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.RectF;
import android.view.MotionEvent;
import android.view.View;

/** Landscape, edge-to-edge touch HUD drag editor. Saves positions only on release. */
final class TouchLayoutView extends View {
    private final TouchSettings settings;
    private final Runnable close;
    private final Paint p=new Paint(Paint.ANTI_ALIAS_FLAG);
    private int dragging=-1, pointer=-1, selected=-1;
    private float downX,downY,lastDragX,lastDragY;
    private float dpadCenterX(){float v=0;for(int i=0;i<4;i++)v+=settings.x[i];return v*.25f;}
    private float dpadCenterY(){float v=0;for(int i=0;i<4;i++)v+=settings.y[i];return v*.25f;}
    TouchLayoutView(Context c,TouchSettings s,Runnable close){super(c);settings=s;this.close=close;
        setFocusable(true);setFocusableInTouchMode(true);
    }
    private void color(int c){p.setColor(c);p.setStyle(Paint.Style.FILL);}
    private void box(Canvas c,float x,float y,float w,float h,int fill){color(fill);c.drawRoundRect(new RectF(x,y,x+w,y+h),8f,8f,p);}
    private void text(Canvas c,String label,float x,float y,float size,int tint,boolean center){
        color(tint);p.setTypeface(android.graphics.Typeface.create("monospace",android.graphics.Typeface.BOLD));p.setTextSize(size);
        if(center)x-=p.measureText(label)/2f;
        c.drawText(label,x,y,p);
    }
    private float radius(int w,int h){return Math.min(w,h)*.050f*settings.scale;}
    private float buttonRadius(int i,int w,int h){return radius(w,h)*settings.sizes[i];}
    private float padRadius(int w,int h){return Math.min(w,h)*.118f*settings.scale*settings.dpadSize;}
    @Override protected void onDraw(Canvas c){super.onDraw(c);
        int w=getWidth(),h=getHeight();c.drawColor(Color.rgb(9,17,16));
        for(int yy=0;yy<h;yy+=36){color(0xff12241b);c.drawRect(0,yy,w,yy+1,p);}
        float top=Math.max(58,h*.14f);
        box(c,10,9,w*.56f,top-17,0xff153824);
        text(c,"CONTROL LAYOUT",23,Math.min(38,top-16),Math.min(24,h*.050f),0xffc0ffbd,false);
        text(c,"TAP, DRAG OR SELECT TO RESIZE",23,Math.min(64,top-3),Math.min(15,h*.029f),0xffd9dfda,false);
        box(c,w-186,10,176,Math.max(43,top-18),0xff367548);
        text(c,"SAVE / BACK",w-98,Math.min(44,top-14),Math.min(19,h*.037f),Color.WHITE,true);
        float r=radius(w,h);
        if(settings.dpadStyle!=0){
            float x=dpadCenterX()*w,y=dpadCenterY()*h;
            color(0x884a9957);c.drawCircle(x,y,padRadius(w,h),p);
            p.setStyle(Paint.Style.STROKE);p.setStrokeWidth(Math.max(2,r*.07f));p.setColor(0xffc2ffb7);
            c.drawCircle(x,y,padRadius(w,h),p);p.setStyle(Paint.Style.FILL);
            text(c,settings.dpadStyle==2?"STICK":"8-WAY",x,y+7,18,Color.WHITE,true);
        }
        for(int i=0;i<settings.x.length;i++){
            if(i<4&&settings.dpadStyle!=0)continue;
            float x=settings.x[i]*w,y=settings.y[i]*h;
            r=buttonRadius(i,w,h);
            color(i==selected?0xaabe9b37:0x993a914d);c.drawCircle(x,y,r,p);
            p.setStyle(Paint.Style.STROKE);p.setStrokeWidth(Math.max(2,r*.07f));p.setColor(0xffc2ffb7);
            c.drawCircle(x,y,r,p);p.setStyle(Paint.Style.FILL);
            String label=TouchSettings.HUD_LABELS[i];
            float sz=Math.min(r*.45f,19);
            // Long action names must fit inside individually resized circles.
            p.setTypeface(android.graphics.Typeface.create("monospace",android.graphics.Typeface.BOLD));
            p.setTextSize(sz);
            float textWidth=p.measureText(label);
            if(textWidth>r*1.75f)sz*=r*1.75f/textWidth;
            text(c,label,x,y+sz*.3f,sz,Color.WHITE,true);
        }
        float barY=top+6;
        box(c,10,barY,110,40,0xff203d29);text(c,"RESET",65,barY+26,19,0xffffffff,true);
        box(c,w*.5f-94,barY,46,40,0xff203d29);text(c,"-",w*.5f-71,barY+28,28,0xffffffff,true);
        box(c,w*.5f+48,barY,46,40,0xff203d29);text(c,"+",w*.5f+71,barY+28,28,0xffffffff,true);
        text(c,selected==-2?"STICK SIZE "+Math.round(settings.dpadSize*100)+"%":
             selected>=0?TouchSettings.LABELS[selected]+" SIZE "+Math.round(settings.sizes[selected]*100)+"%":
             "SELECT A CONTROL",w*.5f,barY-7,16,0xffc0ffbd,true);
        box(c,w-148,barY,138,40,0xff203d29);
        text(c,settings.enabled?"HUD ON":"HUD OFF",w-79,barY+26,16,0xffc0ffbd,true);
    }
    private void resize(float delta){
        if(selected==-2)settings.dpadSize=TouchSettings.clamp(settings.dpadSize+delta,.45f,1.7f);
        else if(selected>=0)settings.sizes[selected]=TouchSettings.clamp(settings.sizes[selected]+delta,.4f,1.7f);
    }
    @Override public boolean onTouchEvent(MotionEvent e){
        int action=e.getActionMasked(),w=getWidth(),h=getHeight();if(w==0||h==0)return true;
        if(action==MotionEvent.ACTION_DOWN){
            downX=e.getX();downY=e.getY();pointer=e.getPointerId(0);dragging=-1;
            if(settings.dpadStyle!=0){
                float dx=downX-dpadCenterX()*w,dy=downY-dpadCenterY()*h,lim=padRadius(w,h)*1.2f;
                if(dx*dx+dy*dy<lim*lim){selected=-2;dragging=-2;lastDragX=downX/w;lastDragY=downY/h;invalidate();return true;}
            }
            float best=Float.MAX_VALUE;
            for(int i=0;i<settings.x.length;i++){
                float dx=downX-settings.x[i]*w,dy=downY-settings.y[i]*h,dist=dx*dx+dy*dy;
                if(dist<buttonRadius(i,w,h)*buttonRadius(i,w,h)*1.55f &&dist<best){dragging=i;selected=i;best=dist;}
            }
            invalidate();return true;
        }
        if(action==MotionEvent.ACTION_MOVE&&dragging==-2){
            int index=e.findPointerIndex(pointer);
            if(index>=0){
                float x=e.getX(index)/w,y=e.getY(index)/h;
                float dx=x-lastDragX,dy=y-lastDragY;
                for(int i=0;i<4;i++){
                    settings.x[i]=TouchSettings.clamp(settings.x[i]+dx,.035f,.965f);
                    settings.y[i]=TouchSettings.clamp(settings.y[i]+dy,.16f,.965f);
                }
                lastDragX=x;lastDragY=y;invalidate();
            }
            return true;
        }
        if(action==MotionEvent.ACTION_MOVE&&dragging>=0){
            int index=e.findPointerIndex(pointer);
            if(index>=0){settings.x[dragging]=TouchSettings.clamp(e.getX(index)/w,.035f,.965f);
                settings.y[dragging]=TouchSettings.clamp(e.getY(index)/h,.16f,.965f);invalidate();}
            return true;
        }
        if(action==MotionEvent.ACTION_UP||action==MotionEvent.ACTION_CANCEL){
            if(dragging>=0||dragging==-2){settings.write();dragging=-1;invalidate();return true;}
            if(action==MotionEvent.ACTION_CANCEL)return true;
            float x=e.getX(),y=e.getY(),top=Math.max(58,h*.14f);
            if(y<top && x>w-186){settings.write();close.run();return true;}
            float barY=top+6;
            if(y>barY&&y<barY+46){
                if(x<125){settings.reset();selected=-1;}
                else if(x>w*.5f-100&&x<w*.5f-40)resize(-.1f);
                else if(x>w*.5f+40&&x<w*.5f+100)resize(.1f);
                else if(x>w-160)settings.enabled=!settings.enabled;
                settings.write();invalidate();
            }
            return true;
        }
        return true;
    }
}
