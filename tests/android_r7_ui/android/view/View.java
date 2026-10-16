package android.view; import android.content.Context;import android.graphics.Canvas;
public class View {
  public static int TEST_WIDTH=1804, TEST_HEIGHT=832;
  public View(Context c){}public void setFocusable(boolean f){}public void setFocusableInTouchMode(boolean f){} public void invalidate(){}
  public int getWidth(){return TEST_WIDTH;} public int getHeight(){return TEST_HEIGHT;}
  protected void onDraw(Canvas c){} public boolean onTouchEvent(MotionEvent e){return false;}
}
