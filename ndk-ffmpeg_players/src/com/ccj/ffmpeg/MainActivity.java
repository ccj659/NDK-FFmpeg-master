package com.ccj.ffmpeg;

import java.io.File;

import com.example.ffmpeg_players.R;

import android.app.Activity;
import android.content.res.AssetManager;
import android.os.Bundle;
import android.os.Environment;
import android.view.Surface;
import android.view.View;
import android.widget.ArrayAdapter;
import android.widget.Spinner;
/**
 * 万能解码器,播放视频器
 * @author ccj
 *
 */
public class MainActivity extends Activity {

	private CcjPlayer player;
	private VideoView videoView;
	private Spinner sp_video;

	@Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		setContentView(R.layout.activity_main);
		videoView = (VideoView) findViewById(R.id.video_view);
		sp_video = (Spinner) findViewById(R.id.sp_video);
		player = new CcjPlayer();
		//多种格式的视频列表
		String[] videoArray = getResources().getStringArray(R.array.video_list);
		ArrayAdapter<String> adapter = new ArrayAdapter<String>(this, 
				android.R.layout.simple_list_item_1, 
				android.R.id.text1, videoArray);
		sp_video.setAdapter(adapter);
	}

	public void mPlay(View btn){
		String video = sp_video.getSelectedItem().toString();
		if("音频".equals(video)){
			String input = new File(Environment.getExternalStorageDirectory(),"Live.mp3").getAbsolutePath();
			String output = new File(Environment.getExternalStorageDirectory(),"Live.pcm").getAbsolutePath();
			player.sound(input, output);
			
		}else{
			String input = new File(Environment.getExternalStorageDirectory(),video).getAbsolutePath();
			//Surface传入到Native函数中，用于绘制
			Surface surface = videoView.getHolder().getSurface();
			player.render(input, surface);
			
		}
		
	}
	
	
}
