package com.zamn.android;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Rect;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import java.io.File;
import java.io.FileInputStream;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Locale;

/**
 * Android-only fullscreen adaptive-width menu compositor, modelled on Xbox_Netplay_DrawText,
 * TitleOverlay and menu_panel. No Android Material widgets are visible in menus.
 * Uses exact Xbox 5x7 glyph definitions and nearest-neighbour upscaling.
 */
final class XboxMenuView extends View {
    static final int W=256,H=224; // W is the ROM title-image width, not the fullscreen menu width
    private static final int WHITE=0xffffffff,GREEN=0xff68ff68,YELLOW=0xffffd35a;
    private static final int MUTED=0xffc8d4dc,BORDER=0xff408040,DARK=0xff001800;
    private static final HashMap<Character,int[]> GLYPHS=new HashMap<>();
    static {
        GLYPHS.put('A', new int[]{14,17,17,31,17,17,17});
        GLYPHS.put('B', new int[]{30,17,17,30,17,17,30});
        GLYPHS.put('C', new int[]{14,17,16,16,16,17,14});
        GLYPHS.put('D', new int[]{30,17,17,17,17,17,30});
        GLYPHS.put('E', new int[]{31,16,16,30,16,16,31});
        GLYPHS.put('F', new int[]{31,16,16,30,16,16,16});
        GLYPHS.put('G', new int[]{14,17,16,23,17,17,15});
        GLYPHS.put('H', new int[]{17,17,17,31,17,17,17});
        GLYPHS.put('I', new int[]{31,4,4,4,4,4,31});
        GLYPHS.put('J', new int[]{7,2,2,2,18,18,12});
        GLYPHS.put('K', new int[]{17,18,20,24,20,18,17});
        GLYPHS.put('L', new int[]{16,16,16,16,16,16,31});
        GLYPHS.put('M', new int[]{17,27,21,21,17,17,17});
        GLYPHS.put('N', new int[]{17,25,21,19,17,17,17});
        GLYPHS.put('O', new int[]{14,17,17,17,17,17,14});
        GLYPHS.put('P', new int[]{30,17,17,30,16,16,16});
        GLYPHS.put('Q', new int[]{14,17,17,17,21,18,13});
        GLYPHS.put('R', new int[]{30,17,17,30,20,18,17});
        GLYPHS.put('S', new int[]{15,16,16,14,1,1,30});
        GLYPHS.put('T', new int[]{31,4,4,4,4,4,4});
        GLYPHS.put('U', new int[]{17,17,17,17,17,17,14});
        GLYPHS.put('V', new int[]{17,17,17,17,17,10,4});
        GLYPHS.put('W', new int[]{17,17,17,21,21,21,10});
        GLYPHS.put('X', new int[]{17,17,10,4,10,17,17});
        GLYPHS.put('Y', new int[]{17,17,10,4,4,4,4});
        GLYPHS.put('Z', new int[]{31,1,2,4,8,16,31});
        GLYPHS.put('0', new int[]{14,17,19,21,25,17,14});
        GLYPHS.put('1', new int[]{4,12,4,4,4,4,14});
        GLYPHS.put('2', new int[]{14,17,1,2,4,8,31});
        GLYPHS.put('3', new int[]{30,1,1,14,1,1,30});
        GLYPHS.put('4', new int[]{2,6,10,18,31,2,2});
        GLYPHS.put('5', new int[]{31,16,16,30,1,1,30});
        GLYPHS.put('6', new int[]{14,16,16,30,17,17,14});
        GLYPHS.put('7', new int[]{31,1,2,4,8,8,8});
        GLYPHS.put('8', new int[]{14,17,17,14,17,17,14});
        GLYPHS.put('9', new int[]{14,17,17,15,1,1,14});
        GLYPHS.put('.', new int[]{0,0,0,0,0,12,12});
        GLYPHS.put(':', new int[]{0,12,12,0,12,12,0});
        GLYPHS.put('-', new int[]{0,0,0,31,0,0,0});
        GLYPHS.put('>', new int[]{16,8,4,2,4,8,16});
        GLYPHS.put('/', new int[]{1,2,2,4,8,8,16});
        GLYPHS.put('<', new int[]{1,2,4,8,4,2,1});
        GLYPHS.put('%', new int[]{25,26,2,4,8,11,19});
    }
    interface ChoiceChanged { void onChange(int index); }
    static final class Entry {
        String label;final Runnable action;final boolean clickable;
        String[] choices;String choiceTitle;int choiceIndex;ChoiceChanged changed;
        Entry(String label,Runnable action,boolean clickable){this.label=label;this.action=action;this.clickable=clickable;}
        String scoreName, scoreTime, scoreDate;
        int scoreRank;
        boolean isChoice(){return choices!=null && choices.length>0;}
        boolean isScore(){return scoreName!=null;}
    }
    private final ArrayList<Entry> entries=new ArrayList<>();
    private int canvasWidth=W;
    private int[] pixels=new int[W*H];
    private Bitmap bmp=Bitmap.createBitmap(W,H,Bitmap.Config.ARGB_8888);
    private final Paint imagePaint=new Paint();
    // Must support menus with >32 entries (e.g. 56-level quick picker).
    private final ArrayList<Rect> locations=new ArrayList<>();
    private int selected=0,scroll=0,countVisible=0;
    private float touchDownX,touchDownY,dragLastY,scrollAccumulator;
    interface SwipeAction { void onSwipe(int levelStep); }
    private SwipeAction horizontalSwipeAction;
    void setHorizontalSwipeAction(SwipeAction action){horizontalSwipeAction=action;}
    private int touchDownEntry=-1;
    private Runnable backAction;
    private boolean swiping;
    private static final int ROWS=6;
    private boolean isMainOrOnline=false;
    private String page="",note="",status="";
    private int[] backdrop;

    XboxMenuView(Context context){super(context);setFocusable(true);setFocusableInTouchMode(true);
        imagePaint.setFilterBitmap(false);
    }
    void setBackdrop(File data){
        File f=new File(data,"zamn_title_256x224.xrgb");
        if(!f.isFile() || f.length()!=W*H*4L)return;
        try(FileInputStream in=new FileInputStream(f)){
            byte[] raw=new byte[W*H*4];int at=0,n;
            while(at<raw.length && (n=in.read(raw,at,raw.length-at))>0)at+=n;
            if(at!=raw.length)return;
            int[] colors=new int[W*H];ByteBuffer buf=ByteBuffer.wrap(raw).order(ByteOrder.LITTLE_ENDIAN);
            for(int i=0;i<colors.length;i++)colors[i]=0xff000000|(buf.getInt()&0xffffff);
            backdrop=colors;
        }catch(Exception ignored){backdrop=null;}
    }
    void reset(String title,String subtitle){page=title;note=subtitle;status="";
        isMainOrOnline="UNDEAD NEIGHBOR EATERS".equals(page)||"ONLINE".equals(page);
        entries.clear();selected=scroll=0;horizontalSwipeAction=null;invalidate();
    }
    void setBackAction(Runnable callback){backAction=callback;}
    Entry add(String label,Runnable action){Entry row=new Entry(label,action,true);entries.add(row);invalidate();return row;}
    Entry addChoice(String title,String[] values,int current,ChoiceChanged change,Runnable openList){
        if(values==null||values.length==0)throw new IllegalArgumentException("Choice must have values");
        Entry e=new Entry(title,openList,true);
        e.choiceTitle=title;e.choices=values.clone();e.choiceIndex=Math.max(0,Math.min(values.length-1,current));e.changed=change;
        entries.add(e);invalidate();return e;
    }
    void focusEntry(int index){
        if(index<0||index>=entries.size()||!entries.get(index).clickable)return;
        selected=index;scroll=Math.max(0,Math.min(index,Math.max(0,entries.size()-ROWS)));
        invalidate();
    }
    private boolean changeChoice(Entry e,int delta){
        if(e==null||!e.isChoice())return false;
        e.choiceIndex=(e.choiceIndex+delta%e.choices.length+e.choices.length)%e.choices.length;
        if(e.changed!=null)e.changed.onChange(e.choiceIndex);
        invalidate();return true;
    }
    boolean adjustSelected(int delta){
        if(selected<0||selected>=entries.size())return false;
        return changeChoice(entries.get(selected),delta);
    }
    void addInfo(String label){entries.add(new Entry(label,null,false));invalidate();}
    void addScore(int rank,String name,String time,String date){
        Entry e=new Entry("",null,false);
        e.scoreRank=rank;e.scoreName=name==null?"":name;
        e.scoreTime=time==null?"":time;e.scoreDate=date==null?"":date;
        entries.add(e);invalidate();
    }
    // Keep the 5x7 Xbox glyphs square: the logical width follows the actual
    // Android screen aspect ratio. Only the launcher uses this framebuffer.
    private void ensureCanvas(){
        if(getWidth()<=0||getHeight()<=0)return;
        int newWidth=Math.max(W,Math.min(640,Math.round((float)H*getWidth()/getHeight())));
        if(newWidth!=canvasWidth){
            canvasWidth=newWidth;pixels=new int[canvasWidth*H];
            bmp=Bitmap.createBitmap(canvasWidth,H,Bitmap.Config.ARGB_8888);
        }
    }
    private void background(){
        for(int y=0;y<H;y++)for(int x=0;x<canvasWidth;x++){
            int t=((x/14+y/10)&1);
            pixels[y*canvasWidth+x]=t==0?0xff081911:0xff0b2017;
        }
        if(backdrop!=null){
            int offset=(canvasWidth-W)/2;
            for(int y=0;y<H;y++)System.arraycopy(backdrop,y*W,pixels,y*canvasWidth+offset,W);
        }
    }
    void clearInfo(){entries.removeIf(row->!row.clickable);invalidate();}
    void setStatus(String value){status=value==null?"":value;invalidate();}
    void setEntryLabel(Entry row,String label){row.label=label;invalidate();}
    private void rect(int x,int y,int w,int h,int color){
        int x1=Math.max(0,x),x2=Math.min(canvasWidth,x+w),y1=Math.max(0,y),y2=Math.min(H,y+h);
        for(int yy=y1;yy<y2;yy++)for(int xx=x1;xx<x2;xx++)pixels[yy*canvasWidth+xx]=color;
    }
    private void glyph(int x,int y,char ch,int color){
        int[] rows=GLYPHS.get(Character.toUpperCase(ch));if(rows==null)return;
        for(int r=0;r<7;r++){int bits=rows[r];for(int c=0;c<5;c++)if((bits&(1<<(4-c)))!=0)rect(x+c,y+r,1,1,color);}
    }
    private void line(int x,int y,String text,int color,int maxChars){
        if(text==null)return;
        text=text.toUpperCase(Locale.US).replace('\u2022','/').replace('\u2013','-').replace('\u2014','-');
        for(int i=0;i<text.length()&&i<maxChars;i++)glyph(x+i*6,y,text.charAt(i),color);
    }
    private String trim(String str,int max){
        if(str==null)return "";
        String s=str.toUpperCase(Locale.US).replace('\n',' ');
        return s.length()>max?s.substring(0,max-1)+">":s;
    }
    private void wrap(String message,int x,int y,int width,int maxRows,int color){
        if(message==null)return;
        String[] segments=message.toUpperCase(Locale.US).replace('\u2022','/').replace('\u2013','-').split("\\n",-1);
        int row=0;
        for(String segment:segments){
            String[] words=segment.split(" +",-1);StringBuilder sb=new StringBuilder();
            for(String word:words){
                if(word.length()==0)continue;
                if(sb.length()>0 && sb.length()+word.length()+1>width){
                    line(x,y+row*9,sb.toString(),color,width);if(++row>=maxRows)return;sb.setLength(0);
                }
                if(sb.length()>0)sb.append(' ');
                sb.append(word);
                while(sb.length()>width){line(x,y+row*9,sb.substring(0,width),color,width);if(++row>=maxRows)return;sb.delete(0,width);}
            }
            if(sb.length()>0){line(x,y+row*9,sb.toString(),color,width);if(++row>=maxRows)return;}
        }
    }
    private boolean titlePage(){return isMainOrOnline;}
    private boolean mainPage(){return "UNDEAD NEIGHBOR EATERS".equals(page);}
    // Larger logical touch targets keep the original pixel font and green title palette,
    // but do not force thumb taps onto 7px-high Xbox text labels.
    private Rect titleTile(int index){
        int columns=mainPage()?2:3;
        int gap=8,side=12;
        int tileWidth=(canvasWidth-side*2-(columns-1)*gap)/columns;
        int col=index%columns,row=index/columns;
        int x=side+col*(tileWidth+gap),y=152+row*30;
        return new Rect(x,y,x+tileWidth,y+26);
    }
    private void titleMenu(){
        background();
        if(backdrop==null){
            line((canvasWidth-15*6)/2,29,"UNDEAD NEIGHBOR",YELLOW,27);
            line((canvasWidth-6*6)/2,43,"EATERS",GREEN,20);
            line((canvasWidth-12*6)/2,60,"ANDROID PORT",MUTED,20);
        }
        rect(0,146,canvasWidth,78,0xe8001608);
        for(int i=0;i<entries.size();i++){
            Entry e=entries.get(i);Rect tile=titleTile(i);boolean picked=i==selected;
            rect(tile.left,tile.top,tile.width(),tile.height(),picked?0xff245e36:0xff0a2b19);
            rect(tile.left,tile.top,tile.width(),1,picked?WHITE:BORDER);
            rect(tile.left,tile.bottom-1,tile.width(),1,picked?WHITE:BORDER);
            rect(tile.left,tile.top,1,tile.height(),picked?WHITE:BORDER);
            rect(tile.right-1,tile.top,1,tile.height(),picked?WHITE:BORDER);
            int maxChars=Math.max(1,(tile.width()-8)/6);
            String label=trim(e.label,maxChars);int ink=picked?WHITE:GREEN;
            if(e.label.length()>maxChars){
                String full=e.label.toUpperCase(Locale.US);
                int split=full.lastIndexOf(' ',maxChars);
                if(split>0&&full.length()-split-1<=maxChars){
                    String a=full.substring(0,split),b=full.substring(split+1);
                    line(tile.left+(tile.width()-a.length()*6)/2,tile.top+4,a,ink,maxChars);
                    line(tile.left+(tile.width()-b.length()*6)/2,tile.top+15,b,ink,maxChars);
                }else line(tile.left+(tile.width()-label.length()*6)/2,tile.top+10,label,ink,maxChars);
            }else line(tile.left+(tile.width()-label.length()*6)/2,tile.top+10,label,ink,maxChars);
            locations.set(i,new Rect(tile.left,tile.top-2,tile.right,tile.bottom+2));
        }
        if(!status.isEmpty()){
            rect(8,106,canvasWidth-16,37,0xe8001800);
            wrap(status,15,111,Math.max(10,(canvasWidth-30)/6),3,WHITE);
        }
    }
    private void drawScore(Entry e,int y){
        int w=canvasWidth;
        String rank=String.format(Locale.US,"%d.",e.scoreRank);
        line(29,y+6,rank,YELLOW,4);
        // Wide phones: four clean columns (rank, name/team, MM:SS.mmm, date).
        if(w>=398){
            int timeX=w-174,dateX=w-93;
            line(56,y+6,trim(e.scoreName,Math.max(8,(timeX-66)/6)),WHITE,Math.max(8,(timeX-66)/6));
            line(timeX,y+6,trim(e.scoreTime,13),GREEN,13);
            line(dateX,y+6,trim(e.scoreDate,11),MUTED,11);
        }else{
            // Short landscape screens use two lines instead of cutting off data.
            line(57,y+2,trim(e.scoreName,Math.max(8,(w-93)/6)),WHITE,Math.max(8,(w-93)/6));
            line(57,y+11,trim(e.scoreTime,13),GREEN,13);
            line(w-93,y+11,trim(e.scoreDate,11),MUTED,11);
        }
    }
    private void interiorMenu(){
        background();
        int w=canvasWidth;
        for(int y=9;y<219;y++)for(int x=10;x<w-10;x++){
            int prev=pixels[y*w+x];
            int r=((prev>>16)&255)>>2,g=((prev>>8)&255)>>2,b=(prev&255)>>2;
            pixels[y*w+x]=0xff000000|(r<<16)|(g<<8)|b;
        }
        rect(10,9,w-20,1,BORDER);rect(10,218,w-20,1,BORDER);
        rect(10,9,1,210,BORDER);rect(w-11,9,1,210,BORDER);
        line(22,21,trim(page,(w-44)/6),WHITE,(w-44)/6);
        if(!note.isEmpty())line(18,40,trim(note,(w-36)/6),YELLOW,(w-36)/6);
        int visible=Math.max(0,Math.min(ROWS,entries.size()-scroll));countVisible=visible;
        for(int slot=0;slot<visible;slot++){
            int i=scroll+slot;Entry e=entries.get(i);int y=54+slot*22;
            boolean chosen=e.clickable&&i==selected;
            rect(17,y,w-34,19,chosen?0xff205d38:(slot%2==0?0xd9002514:0xd9001d12));
            rect(17,y,2,19,chosen?WHITE:BORDER);
            int color=e.clickable?(chosen?WHITE:GREEN):MUTED;
            if(e.isScore()){
                drawScore(e,y);
            }else if(e.isChoice()){
                int arrowColor=chosen?WHITE:GREEN;
                rect(19,y+1,37,17,chosen?0xff316c44:0xff15402a);
                rect(w-57,y+1,38,17,chosen?0xff316c44:0xff15402a);
                line(35,y+6,"<",arrowColor,1);line(w-42,y+6,">",arrowColor,1);
                String choice=trim(e.choices[e.choiceIndex],Math.max(14,(w-125)/6));
                line((w-Math.min(e.choiceTitle.length(),21)*6)/2,y+2,trim(e.choiceTitle,21),YELLOW,21);
                line((w-choice.length()*6)/2,y+11,choice,color,Math.max(14,(w-125)/6));
            }else{
                if(e.clickable&&chosen)line(22,y+6,">",WHITE,1);
                int maxChars=Math.max(1,(w-51)/6);
                line(30,y+6,trim(e.label,maxChars),color,maxChars);
            }
            locations.set(i,new Rect(17,y,w-17,y+20));
        }
        if(scroll>0)line(w-33,48,"<",GREEN,1);
        if(scroll+visible<entries.size())line(w-33,185,">",GREEN,1);
        rect(14,192,w-28,1,BORDER);
        rect(w-77,195,63,20,0xff184c2d);
        rect(w-77,195,63,1,BORDER);rect(w-77,214,63,1,BORDER);
        line(w-63,202,"BACK",WHITE,4);
        String footer=status.isEmpty()?"A SELECT  B BACK":status;
        wrap(footer,18,196,Math.max(14,(w-116)/6),2,MUTED);
    }
    @Override protected void onDraw(Canvas c){
        super.onDraw(c);ensureCanvas();
        locations.clear();for(int i=0;i<entries.size();i++)locations.add(null);
        if(titlePage())titleMenu();else interiorMenu();
        bmp.setPixels(pixels,0,canvasWidth,0,0,canvasWidth,H);
        c.drawColor(Color.BLACK);
        // Menus fill the whole landscape device; gameplay stays in SDL's original ratio.
        c.drawBitmap(bmp,null,new Rect(0,0,getWidth(),getHeight()),imagePaint);
    }
    private int nextActive(int target,int step){
        if(entries.isEmpty())return -1;
        for(int n=0;n<entries.size();n++){
            target=(target+step+entries.size())%entries.size();
            if(entries.get(target).clickable)return target;
        }
        return -1;
    }
    void navigate(int direction){
        int pos=nextActive(selected,direction);
        if(pos>=0){selected=pos;
            if(!titlePage()){
                if(selected<scroll)scroll=selected;
                if(selected>=scroll+ROWS)scroll=selected-ROWS+1;
            }
            invalidate();
        }
    }
    void navigateVertical(int direction){
        if(titlePage()){
            int stride=mainPage()?2:3;
            int target=selected+direction*stride;
            if(target>=0&&target<entries.size()&&entries.get(target).clickable){
                selected=target;invalidate();return;
            }
        }
        navigate(direction);
    }
    void activate(){if(selected>=0&&selected<entries.size()){Entry e=entries.get(selected);if(e.clickable&&e.action!=null)e.action.run();}}
    private int hitEntry(float bx,float by){
        for(int i=0;i<locations.size();i++){
            Rect r=locations.get(i);
            if(r!=null && r.contains((int)bx,(int)by) && entries.get(i).clickable)return i;
        }
        return -1;
    }
    private boolean backHit(float bx,float by){return !titlePage()&&bx>=canvasWidth-80&&bx<=canvasWidth-10&&by>=193&&by<=220;}
    @Override public boolean onTouchEvent(MotionEvent event){
        if(getWidth()<=0||getHeight()<=0)return true;
        ensureCanvas();
        float bx=event.getX()*canvasWidth/getWidth();
        float by=event.getY()*H/getHeight();
        int type=event.getActionMasked();
        if(type==MotionEvent.ACTION_DOWN){
            touchDownX=bx;touchDownY=by;dragLastY=by;scrollAccumulator=0;
            touchDownEntry=hitEntry(bx,by);swiping=false;
            return true;
        }
        if(type==MotionEvent.ACTION_MOVE){
            if(!titlePage() && Math.abs(by-touchDownY)>6 &&
                 Math.abs(by-touchDownY)>Math.abs(bx-touchDownX)*1.2f){
                swiping=true;
                // Perform scrolling DURING the drag instead of waiting for finger-up.
                scrollAccumulator+=dragLastY-by;
                int steps=(int)(scrollAccumulator/16f);
                if(steps!=0){
                    scroll=Math.max(0,Math.min(Math.max(0,entries.size()-ROWS),scroll+steps));
                    scrollAccumulator-=steps*16f;
                    invalidate();
                }
            }
            dragLastY=by;
            return true;
        }
        if(type==MotionEvent.ACTION_CANCEL){touchDownEntry=-1;swiping=false;return true;}
        if(type==MotionEvent.ACTION_UP){
            float dx=bx-touchDownX,dy=by-touchDownY;
            int start=touchDownEntry;touchDownEntry=-1;
            if(!titlePage() && horizontalSwipeAction!=null && Math.abs(dx)>24
                  && Math.abs(dx)>Math.abs(dy)*1.15f){
                horizontalSwipeAction.onSwipe(dx<0?1:-1);return true;
            }
            if(!titlePage() && start>=0 && entries.get(start).isChoice()
                    && Math.abs(dx)>15 && Math.abs(dx)>Math.abs(dy)*1.25f){
                selected=start;changeChoice(entries.get(start),dx<0?1:-1);return true;
            }
            if(!titlePage() && swiping){
                for(int j=scroll;j<Math.min(entries.size(),scroll+ROWS);j++){
                    if(entries.get(j).clickable){selected=j;break;}
                }
                invalidate();return true;
            }
            if(backHit(bx,by)){
                if(backAction!=null)backAction.run();return true;
            }
            int i=hitEntry(bx,by);
            if(i>=0){
                Entry e=entries.get(i);selected=i;invalidate();
                if(e.isChoice()){
                    Rect r=locations.get(i);
                    if(bx<r.left+40){changeChoice(e,-1);return true;}
                    if(bx>=r.right-40){changeChoice(e,1);return true;}
                    // Center opens the full options list, including direct level selection.
                }
                activate();return true;
            }
            // Tap top/bottom scroll affordances on the right side.
            if(!titlePage()&&bx>canvasWidth-45&&by>40&&by<54&&scroll>0){
                scroll=Math.max(0,scroll-ROWS);invalidate();return true;
            }
            if(!titlePage()&&bx>canvasWidth-45&&by>=180&&by<192&&scroll+ROWS<entries.size()){
                scroll=Math.min(entries.size()-ROWS,scroll+ROWS);invalidate();return true;
            }
        }
        return true;
    }
}
