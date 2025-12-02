package com.beatchess.android;

import android.content.Context;
import android.os.Bundle;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.MotionEvent;
import android.os.Handler;
import android.os.Looper;
import androidx.appcompat.app.AppCompatActivity;

public class ChessActivity extends AppCompatActivity implements SurfaceHolder.Callback {
    
    private ChessEngine engine;
    private ChessSurfaceView surfaceView;
    private Handler mainHandler;
    
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_chess);
        
        mainHandler = new Handler(Looper.getMainLooper());
        
        engine = new ChessEngine();
        engine.nativeInitialize();
        
        surfaceView = findViewById(R.id.chess_surface);
        surfaceView.setEngine(engine);
        surfaceView.setCallback(this);
    }
    
    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        // Pass native window to engine for Cairo rendering
        engine.nativeSetNativeWindow(holder.getSurface());
    }
    
    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
        // Surface dimensions changed
        surfaceView.startRenderThread(width, height);
    }
    
    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
        surfaceView.stopRenderThread();
    }
    
    @Override
    protected void onDestroy() {
        super.onDestroy();
        if (engine != null) {
            engine.nativeCleanup();
        }
    }
}

/**
 * Custom SurfaceView for Cairo-based chess rendering
 * Uses a separate rendering thread for smooth animation
 */
class ChessSurfaceView extends SurfaceView implements SurfaceHolder.Callback {
    
    private ChessEngine engine;
    private RenderThread renderThread;
    private volatile boolean isRunning = false;
    
    public ChessSurfaceView(Context context) {
        super(context);
        initView();
    }
    
    public ChessSurfaceView(Context context, android.util.AttributeSet attrs) {
        super(context, attrs);
        initView();
    }
    
    private void initView() {
        getHolder().addCallback(this);
        setFocusable(true);
        setFocusableInTouchMode(true);
    }
    
    public void setEngine(ChessEngine engine) {
        this.engine = engine;
    }
    
    public void startRenderThread(int width, int height) {
        if (renderThread != null && renderThread.isAlive()) {
            stopRenderThread();
        }
        
        isRunning = true;
        renderThread = new RenderThread(this, engine, width, height);
        renderThread.start();
    }
    
    public void stopRenderThread() {
        isRunning = false;
        if (renderThread != null) {
            try {
                renderThread.join();
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
            }
        }
    }
    
    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        // Surface created, ready to render
    }
    
    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
        startRenderThread(width, height);
    }
    
    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
        stopRenderThread();
    }
    
    @Override
    public boolean onTouchEvent(MotionEvent event) {
        if (engine != null && event.getAction() == MotionEvent.ACTION_DOWN) {
            int width = getWidth();
            int height = getHeight();
            engine.nativeHandleTouch((int)event.getX(), (int)event.getY(), width, height);
            return true;
        }
        return super.onTouchEvent(event);
    }
    
    /**
     * Rendering thread that calls Cairo drawing code via JNI
     */
    private static class RenderThread extends Thread {
        private ChessSurfaceView view;
        private ChessEngine engine;
        private int width, height;
        private volatile boolean shouldRun = true;
        private long lastFrameTime = 0;
        private static final long FRAME_DELAY = 16; // ~60 FPS
        
        RenderThread(ChessSurfaceView view, ChessEngine engine, int width, int height) {
            this.view = view;
            this.engine = engine;
            this.width = width;
            this.height = height;
            setName("ChessRenderThread");
        }
        
        @Override
        public void run() {
            lastFrameTime = System.currentTimeMillis();
            
            while (shouldRun && view.isRunning) {
                long currentTime = System.currentTimeMillis();
                double deltaTime = (currentTime - lastFrameTime) / 1000.0; // Convert to seconds
                lastFrameTime = currentTime;
                
                try {
                    // Call Cairo rendering through JNI
                    engine.nativeRender(width, height, deltaTime);
                    
                    // Frame rate limiting
                    long frameTime = System.currentTimeMillis() - currentTime;
                    if (frameTime < FRAME_DELAY) {
                        Thread.sleep(FRAME_DELAY - frameTime);
                    }
                } catch (InterruptedException e) {
                    Thread.currentThread().interrupt();
                    break;
                }
            }
        }
        
        void stopRendering() {
            shouldRun = false;
        }
    }
}
