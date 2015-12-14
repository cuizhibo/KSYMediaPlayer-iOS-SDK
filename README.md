# KSYMediaPlayer-iOS-SDK
---
##SDK支持说明
目前播放器SDK支持的流媒体传输协议有：

* RTMP，HTTP，HLS及RTSP(RTP,SDP)

解码基于FFMPEG，音视频格式支持列表如下（以下仅列出常见格式）

* MP4，3GP，FLV，TS/TP，RMVB ，MKV，M4V，AVI，WMV ，MKV

##SDK使用说明 

###结构
工程中包含2个工程，其中

* KSYMediaPlayer - 播放器SDK
* KSYVideoDemo - 使用播放器SDK的demo

KSYMediaPlayer编译得到的libKSYMediaPlayer.a静态库文件就是播放器SDK，头文件在KSYMediaPlayer目录中

###集成

根据用户的需求，可以选择2种方式：

* 如果仅需要播放器SDK，那么仅需要引入libKSYMediaPlayer.a静态库文件以及对应的头文件即可

* 如果需要一个完整的播放器，可以直接使用Demo，然后根据需求进行修改相应的部分，具体内容参见KSYVideoDemo工程

###错误码对应表
<table>
  <tr>
    <th>错误码</th>
    <th>错误类型</th>
    <th>描述</th>
  </tr>
  <tr>
    <td>10000</td>
    <td>ERROR_UNKNOWN</td>
    <td>未知错误</td>
  </tr>
  <tr>
    <td>10001</td>
    <td>ERROR_IO</td>
    <td>IO错误</td>
  </tr>
  <tr>
    <td>10002</td>
    <td>ERROR_TIMEOUT</td>
    <td>请求超时</td>
  </tr>
 <tr>
    <td>10003</td>
    <td>ERROR_UNSUPPORT</td>
    <td>不支持的格式</td>
  </tr>
 <tr>
    <td>10004</td>
    <td>ERROR_NOFILE</td>
    <td>文件不存在</td>
  </tr>
 <tr>
    <td>10005</td>
    <td>ERROR_SEEKUNSUPPORT</td>
    <td>当前不支持seek</td>
  </tr>
 <tr>
    <td>10006</td>
    <td>ERROR_SEEKUNREACHABLE</td>
    <td>当前seek不可达</td>
  </tr>
 <tr>
    <td>10007</td>
    <td>ERROR_DRM</td>
    <td>DRM出错</td>
  </tr>
 <tr>
    <td>10008</td>
    <td>ERROR_MEM</td>
    <td>内存溢出</td>
  </tr> 
<tr>
    <td>10009</td>
    <td>ERROR_WRONGPARAM</td>
    <td>参数错误</td>
  </tr>
</table>




