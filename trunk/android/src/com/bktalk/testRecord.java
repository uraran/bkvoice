package com.bktalk;

import java.util.LinkedList;
import java.util.concurrent.locks.Lock;
import java.util.concurrent.locks.ReentrantLock;

import android.app.Activity;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioRecord;
import android.media.AudioTrack;
import android.media.MediaRecorder;
import android.os.Bundle;
import android.view.View;
import android.widget.Button;
import android.widget.SeekBar;
import android.widget.Toast;

public class testRecord extends Activity {
	/** Called when the activity is first created. */
	Button btnRecord, btnStop, btnExit;
	SeekBar skbVolume;// 调节音量
	boolean isRecording = false;// 是否录放的标记
	static final int frequency = 44100;
	static final int channelConfiguration = AudioFormat.CHANNEL_CONFIGURATION_MONO;
	static final int audioEncoding = AudioFormat.ENCODING_PCM_16BIT;
	int recBufSize, playBufSize;
	AudioRecord audioRecord;
	AudioTrack audioTrack;
	public LinkedList<byte[]> dataList = new LinkedList<byte[]>();
	Lock lock = new ReentrantLock();

	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		setContentView(R.layout.testrecord);
		setTitle("助听器");
		recBufSize = AudioRecord.getMinBufferSize(frequency,
				channelConfiguration, audioEncoding);

		playBufSize = AudioTrack.getMinBufferSize(frequency,
				channelConfiguration, audioEncoding);
		// -----------------------------------------
		audioRecord = new AudioRecord(MediaRecorder.AudioSource.MIC, frequency,
				channelConfiguration, audioEncoding, recBufSize);

		audioTrack = new AudioTrack(AudioManager.STREAM_VOICE_CALL, frequency,
				channelConfiguration, audioEncoding, playBufSize,
				AudioTrack.MODE_STREAM);
		// ------------------------------------------
		btnRecord = (Button) this.findViewById(R.id.btnRecord);
		btnRecord.setOnClickListener(new ClickEvent());
		btnStop = (Button) this.findViewById(R.id.btnStop);
		btnStop.setOnClickListener(new ClickEvent());
		btnExit = (Button) this.findViewById(R.id.btnExit);
		btnExit.setOnClickListener(new ClickEvent());
		skbVolume = (SeekBar) this.findViewById(R.id.skbVolume);
		skbVolume.setMax(100);// 音量调节的极限
		skbVolume.setProgress(70);// 设置seekbar的位置值
		audioTrack.setStereoVolume(0.7f, 0.7f);// 设置当前音量大小
		skbVolume
				.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {

					@Override
					public void onStopTrackingTouch(SeekBar seekBar) {
						float vol = (float) (seekBar.getProgress())
								/ (float) (seekBar.getMax());
						audioTrack.setStereoVolume(vol, vol);// 设置音量
					}

					@Override
					public void onStartTrackingTouch(SeekBar seekBar) {
						// TODO Auto-generated method stub
					}

					@Override
					public void onProgressChanged(SeekBar seekBar,
							int progress, boolean fromUser) {
						// TODO Auto-generated method stub
					}
				});
	}

	@Override
	protected void onDestroy() {
		super.onDestroy();
		android.os.Process.killProcess(android.os.Process.myPid());
	}

	class ClickEvent implements View.OnClickListener {

		@Override
		public void onClick(View v) {
			if (v == btnRecord) {
				isRecording = true;
				new RecordThread().start();// 开一条线程录
				new PlayThread().start();// 开一条线程放
			} else if (v == btnStop) {
				isRecording = false;
			} else if (v == btnExit) {
				isRecording = false;
				testRecord.this.finish();
			}
		}
	}

	class PlayThread extends Thread {
		public void run() {
			audioTrack.play();// 开始播放
			while (isRecording) {
				if (dataList.isEmpty()) {
					try {
						Thread.sleep(200);
					} catch (InterruptedException e) {
						// TODO Auto-generated catch block
						e.printStackTrace();
					}
				} else {
					lock.lock();
					try {
						byte[] tmpBuf = dataList.get(0);
						audioTrack.write(tmpBuf, 0, tmpBuf.length);
						dataList.remove(0);
					} finally {
						// 释放锁
						lock.unlock();
					}
				}
			}
			audioTrack.stop();
		}
	}

	class RecordThread extends Thread {
		public void run() {
			try {
				byte[] buffer = new byte[recBufSize];
				audioRecord.startRecording();// 开始录制

				while (isRecording) {
					// 获取锁
					lock.lock();
					try {
						// 从MIC保存数据到缓冲区
						int bufferReadResult = audioRecord.read(buffer, 0,
								recBufSize);

						dataList.add(buffer);
					} finally {
						// 释放锁
						lock.unlock();
					}
					// byte[] tmpBuf = new byte[bufferReadResult];
					// System.arraycopy(buffer, 0, tmpBuf, 0, bufferReadResult);
					// 写入数据即播放

				}

				audioRecord.stop();
			} catch (Throwable t) {
				Toast.makeText(testRecord.this, t.getMessage(), 1000);
			}
		}
	};
}