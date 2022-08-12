# 流媒体架构介绍

### 业务架构

### 部署架构

# CMS服务

CMS服务主要包含以下模块，下面分别进行分析

## 1 CServerMgr模块

该模块主要功能：

- 支持USG注册请求
- UsgServerMgr模块获取USG的建链信息

通过调用CServerMgr::startupImpl接口启动ServerMgr模块，CServerMgr::startupImpl主要完成以下工作：

1. 调用loadConfigFile加载配置文件config.json，配置分为：

   - httpServer配置项

   - httpClient配置项

   - rimiAddr配置项 

2. 创建CHttpClientMgr对象，并调用CHttpClientMgr::init接口

​		CHttpClientMgr用来发送http请求

3. 创建CHttpServerMgr对象，并调用CHttpServerMgr::init接口

​		cms的http服务器

4. 启动线程执行getStreamStatThread

​		getStreamStatThread中每60s调用httpClient中配置的所有nginx的nginxStatPath配置的接口，比如（"http://192.168.3.147:20003/sys_stat"）查询所有nginx上的流信息，并统计出流负载，通过`DatabaseManager`更新到数据库的`three_streamshareinfo`和`three_hisstreamshareinfo`两个表中

CServerMgr的类关系图如下：

![image-20220812164938599](https://leonfang.oss-cn-hangzhou.aliyuncs.com/image-20220812164938599.png)

### 1.1 CHttpClientMgr

CHttpClientMgr子模块的功能是作为http client，主动发送http请求。比如通知oss服务推送视频和截图到阿里oss，获取nginx的流负载等

### 1.2 CHttpServerMgr

CHttpServerMgr子模块的功能是启动http server，接收并处理用户发起的http请求，比如拉流、断流等。

#### 1.2.1 初始化流程（`CHttpServerMgr::init`）

- 调用RCF::init

- 调用initCmd2ParaMap

- 解析配置文件parseConfig

- 调用setOssAddr设置oss地址

- 调用createHttpServer创建CHttpServer对象，并调用CHttpServer::init

  - `CHttpServer::init`传入参数为`IHttpsServerUserHandlerMgr* userHandlerMgr`，也就是`CHttpServerMgr`对象

  - `CHttpServer::init`根据ip和端口调用boost::asio接口启动http server监听

  - 每收到一个http请求调用回调函数handle_accept，统一通过`CHttpsServerHandler`对象处理，调用`CHttpsServerHandler::start`

    具体处理流程如下：

    ```
    -> CHttpsServerHandler::start()
    -> CHttpsServerHandler::handle_read()
    	-> m_requestParser.parse() // http协议解析
    		-> CHttpRequestParser::handleBodyData()
    		-> CHttpRequestParser::consume()
    	-> m_userHandlerMgr->handleRequest()，也就是回调CHttpServerMgr::handleRequest()
    ```

- 创建httpReqThread线程

  用来处理http请求，1.1.2会进行详细分析

- 创建threadHotResRecover线程

  每10s执行一次，对存在于热点视频列表中的推流失败记录，重新推流

- 创建threadHotResCheck线程

  每10s执行一次，检测热点视频是否正常，以及非热点视频的转码流是否正常

#### 1.2.2 http请求处理流程

http请求回调入口为`CHttpServerMgr::handleRequest()`，新请求放到`m_listReq`中，通过条件变量通知`httpReqThread`线程取出所有http请求处理（<font color='red'>httpReqThread是单线程，存在并发瓶颈</font>）

具体处理流程如下：

```
-> parserHttpReq()
	-> parserHttp() // 解析uri的location和参数
	-> genHttpReply // parserHttp失败
	-> execHttpReq // parserHttp成功，解析para.cmd调用对应的处理函数
	-> req.spHandler->handleResponse()
```

`execHttpReq`中重点分析以下请求：

##### 1.2.2.1 <font color='red'>URL_VIDEOTRANSBEGIN_STR（video.VideoTransBegin）</font>

用户拉流请求，对应的处理函数为`requestStreamBegin`，具体流程如下：

```
-> getParamValue() // 参数解析
-> dm->qryXXX() // 读取数据库进行校验，信息获取
-> getstreamPublishAddr() // 拼接rtmp推流和拉流地址
	-> 如果配置文件未配置streamOrigin项，使用全局配置的rtmpPushAddr和rtmpAddr进行拼接
	-> 如果配置文件已配置streamOrigin项，选取当前流负载最低的streamOrigin拼接
-> getstreamPlayAddr() // 拼接flv和hls波流地址
	-> 如果配置文件未配置streamEdge项，使用全局配置的rtmpAddr进行拼接
	-> 如果配置文件已配置streamEdge项，选取当前流负载最低的streamEdge拼接
-> CUsgServerMgr::playLivingStream() 发送rpc消息到UsgServer
	-> dm->addSessionInfo() 把流信息写入wt_session_info表中
	-> getUsgRimiProxier() 通过usg的注册信息以及rimi获取usg的地址
	-> CCmsServerManager::playLivingStream() 通过rpc跨进程调用
or -> CUsgServerMgr::playLivingStreamAskMdsAddrs() 如果playLivingStream调用失败则到这一步
-> addStreamOrigin() // 流启动成功记录到streamOriginInfo表中
-> transResultToRep() // 生成http响应
```

##### 1.2.2.2 URL_GETSTREAMSESSION（getStreamSession）

获取所有的流信息，对应的处理函数为`requestStreamSession`，调用CDatabaseManager::qryStreamSsid()获取wt_session_info表中所有流返回

##### 1.2.2.3 URL_STREAMPUBLISHNOTIFY（streamPublish）

对应的处理函数为`requestStreamPublish`，只支持参数中`act==STAGE_DONE`，`act`为其他值返回200

从代码上来看`STAGE_DONE`会调用`dm->delStreamOriginInfo`和`dm->delSessionInfo`，功能和URL_STREAMPLAYNOTIFY中类似？

##### 1.2.2.4 <font color='red'>URL_STREAMPLAYNOTIFY（streamPlay）</font>

用户断流或者查询拉流数量是否达到，对应的处理函数为`requestStreamPlay`，具体的实现要根据参数中的`act`来确定

- 如果`act == STAGE_DONE`表示用户断流，设置表`three_streamshareinfo`和`three_hisstreamshareinfo`中的`end_time`

  ```
  -> dm->updateSreamInfo() // 设置表three_streamshareinfo的end_time，并清除cli_id
  -> dm->updateHisSreamInfo() // 设置表three_hisstreamshareinfo的end_time
  -> dm->qryIsOnPlay() // 查询是否有客户端在拉流，如果有则不做操作，没有则到下一步
  -> dm->delSessionInfo() // 从表wt_session_info删除该流
  非热点视频
  -> CUsgServerMgr::stopStream() // 通过rimi框架发送rpc消息给usg断流
  -> dm->delStreamOriginInfo() // 从表streamOriginInfo中删除流
  -> curClient->stopStream() // 通过rcf发送rpc消息给转码sts断流
  ```

- 如果`act != STAGE_DONE`

​		目的是查询该用户拉流请求是否达到video_num_limit限制？并没有获取拉流地址

## 2 CUsgServerMgr模块

主要负责的功能：

- 向USG发送实时流、历史流、历史下载业务请求
- 转发CServerMgr模块的RPC消息，比如`playLivingStream/playLivingStreamAskMdsAddrs`

`startupImpl()`完成以下工作：

- 注册RIMI模块

- 启动reportThread线程

  先根据服务配置的`serverIp`和`serverPort`查询gateway的`id`，然后再查询userID，最终查询得到资源的目录信息，通过http请求推送给用户？

- 启动reportDownloadFinishThread线程

  把usg服务上报的资源信息同步到数据库和OSS

## 3 CDatabaseManager模块

数据库操作模块，封装了对数据库的读写操作，被CMS服务内部的其他模块调用

## 4 CHttpProtocolMgr模块

http协议模块，内部包括了http协议的解析和封装过程，被CMS服务内部的其他模块调用，提供以下接口：

```
createHttpServer()
releaseHttpServer()
createHttpsServer()
releaseHttpsServer()
createHttpsClient()
releaseHttpsClient()
createHttpClient()
releaseHttpClient()
```

## 5 COssServerMgr模块

提供了一些视频下载接口，供CMS服务接受http请求后内部调用，推送到OSS。

`startupImpl()`中启动了以下四个线程：

- reportDownloadInfoTask

  调用`CServerMgr::publishDownload`把视频和截图推送到阿里OSS

- downloadThread

  从`m_downloadTaskList`中取出需要下载的视频信息

- snapThread

  视频下载完成后执行截图操作，<font color='red'>此处的实现是调用system函数执行ffmpeg命令完成</font>

- stopRecordStreamThread

  从m_recordStreamInfos中取出需要停止的流

## 6 CSpsServerMgr模块

负责转发RPC消息到SPS服务对资源进行下载和停止操作。

# USG服务

USG是作为URG和UMG的信令转发服务

`CUsgServerMgr::startupImpl()`中完成以下工作：

- 注册js

- 解析启动命令行的load参数中指定的路径，比如`./run.sh CuiApp --load=../../config/app/load.js`，解析路径下的`config.conf`配置文件

  ```
  192.168.3.245:20000
  192.168.3.245:22000
  12345678901234567890
  192.168.3.245:10000
  test
  000000
  100
  10
  
  #请保留此注释
  #第一行CMS服务器地址
  #第二行本服务器供RG连接的监听地址
  #第三行为USG服务标识符
  #第四行为下级平台的地址和端口
  #第五行为下级平台的用户名
  #第六行为下级平台的密码
  #第七行实时流网关流接入上限
  #其他的行,全认为是注释,不读取.
  ```

- 注册rimi

- 初始化LinkMgr

  LinkMgr作用？

- 初始化UrgServerMgr

  向urg转发CmsServerMgr模块收到的RPC消息

- 初始化CmsServerMgr

- 初始化CWtMgr

  CWtMgr封装了云台管理操作？

## 1 CmsServerMgr模块

该模块提供一系列RPC给cms服务调用，比如：`playLivingStream/doPtzMove`

### 1.1 playLivingStream

用户向cms发起http请求拉流播放，最终转发到urg服务执行

```
-> CCmsServerManager::playLivingStream()
	-> CUsgServerMgr::playLivingStream()
		-> CUrgServerManager::isExistLivingStream() // 判断该流是否已经在拉流中
			-> m_livingStreamMgr.isExistStreamSsid() // CStreamMgr内部维护了一个map来管理流
		-> CUrgServerManager::createOutStreamUrg()
			-> getAllOnLineUrgEpid() // 获取所有在线的urg
			-> createOutLivingStreamService()
				-> m_livingStreamMgr.isExistStreamSsid() // 查询会话是否已经存在于流管理对象中
				-> m_livingStreamMgr.findMinStreamSsid() // 查询流管理对象中负载最低的urg
				-> m_livingStreamMgr.addStreamSsid() // 加入到流管理对象中的缓存
		-> CUrgServerManager::requestStreamToUrg() // 向urg请求流的播放
			-> getIUrgServer() // 获取urg的地址
			-> urgServer->playStream() // 调用urg的rpc接口playStream()
		-> *CUrgServerManager::releaseOutStreamUrg // 向urg请求流播放失败才执行
```

### 1.2 doPtzMove

调用栈如下，

```
-> CCmsServerManager::doPtzMove() // 转发到CUsgServerMgr
	-> CUsgServerMgr::doPtzMove()
		-> CWtMgr::ptzMove() // 执行云台移动操作
        	-> transUuidToResId()
			-> m_manageSession->createPtzController()
			-> convertAsgMoveToMpcMove(oper, mpcOper);
			-> convertAsgSpeedToMpcSpeed(speed, mpcSpeed);
			-> ptz->move( mpcOper, mpcSpeed );
			-> ptz->release();
```

## 2 UrgServerMgr模块

该模块对外提供`serviceRegister()`接口给urg和umg服务根据参数中的type进行注册。

还负责转发RPC消息给urg和umg服务。

# URG服务

实时拉流转推服务

`CUrgServerMgr::startupImpl()`完成以下工作：

- 获取rimi包管理接口

  `app->getServiceInstance(WTOE_SNAME_RimiMechanismProxy_RimiMechanismProxy, rimi)`

- 获取连接管理包服务接口

  `app->getServiceInstance(WTOE_SNAME_LinkMgr_LinkMgr, linkMgr)`

- 解析配置文件

  `serviceType_urg.cfg`配置决定是作为urg还是umg启动

- 创建sg链路状态观察者

  `m_sgPeerLinkObserver = new_o()`

- 登录平台

  `m_linkToWt = new_o()`

- 创建rtmpClientMgr管理者并初始化

  `m_rtmpStreamClientMgr = new_o()`

- 初始化连接管理模块,注册通知

  `m_linkMgr->init()`

- 启动流异常处理线程

- 启动通知处理线程

- 绑定rimi服务

- 增加sg服务地址

## 1 CUrgServerMgr模块

### 1.1 拉流转推`CUrgServerMgr::playStream`

```
-> CUrgServerMgr::playStream()
	-> m_rtmpStreamClientMgr->createRtmpStreamClient()
		-> RtmpStreamHandlerFactory::createRtmpStreamHandler()
		-> CRtmpStreamClient *rtmpStreamClient = new_o(CRtmpStreamClient, ...)
		-> rtmpStreamHandler->init() // CRtmpStreamHandler，下面会详细展开分析
	-> streamHandler = new_o( CStreamHandler,...) // 创建ps流处理对象
	-> livingStream = m_linkToWt->createLivingStream() // 创建实时流对象，用于接收二代平台推送的ps流数据
	-> livingStream->setHandler() 设置实时流数据处理对象
	-> livingStream->init() // 实时流对象初始化
	-> std::pair< StreamHandlerMapIter, bool> ret = m_streamHandlers.insert(std::make_pair(streamSsid, handlerItem)) // 加入到缓存
```

再重点看下`CRtmpStreamHandler::init`

```
-> rtmpStreamHandler->init() // CRtmpStreamHandler
	-> m_streamData.init() // 初始化循环队列，用于接收拉流数据
	-> getTransformParam() // 根据参数判断是从发送ps流到rtmp服务器，还是从rtmp服务器接收ps流
	-> boost::thread( boost::bind( &CRtmpStreamHandler::streamHandlerThread, ...) //启动ffmpeg线程拉流转推
```

```
-> CRtmpStreamHandler::streamHandlerThread()
	-> initInput()
		-> m_fmtCtxIn = avformat_alloc_context() // 为输入流创建AVFormatContext
		-> uint8_t* pBuf = ( uint8_t* )av_mallocz() // 创建接收buffer
		-> AVIOContext* ioCtx = avio_alloc_context() // 创建AVIOContext用于接收数据
		-> av_probe_input_buffer( ioCtx, &inputFmt, NULL, NULL, 0, 0 ) // 探测内存部分数据，确定输入流类型
		-> m_fmtCtxIn->pb = ioCtx;
		-> avformat_open_input() // 打开输入源读取部分数据进行探测，推流时,不输入数据会阻塞在avformat_open_input,依靠使用者主动调用fini()来结束处理，fini()中m_fmtCtxIn->pb->eof_reached = 1;
	-> initOutput()
		-> avformat_alloc_output_context2()
		-> avformat_new_stream()
		-> avcodec_copy_context()
		-> avio_open2()
		-> avformat_write_header()
	-> transform() // 拉流转推处理
		-> av_init_packet( &pkt )
		-> ret = av_read_frame( m_fmtCtxIn, &pkt );
		-> av_rescale_q_rnd()
		-> av_rescale_q()
		-> av_interleaved_write_frame()
		-> av_write_trailer()
	-> finiOutput()
	-> finiInput()
```

拉流转推相关类的关系图：

![image-20220812165026231](https://leonfang.oss-cn-hangzhou.aliyuncs.com/image-20220812165026231.png)

- `createLivingStream`根据`resId`和`streamType`创建实时流对象
- 二代平台调用`CStreamHandler::stream()`推送ps流数据
- `CStreamHandler`对象把数据透传给内部成员变量`CRtmpStreamClient::receiveData()`
- `CRtmpStreamClient`再透传到内部的流处理对象`CRtmpStreamHandler::inputMediaData()`
- `CRtmpStreamHandler`放到循环队列`CCircularQueue`中，等待异步线程中`av_read_frame`读取

### 1.2 断流`CUrgServerMgr::stopStream`

```
-> StreamHandlerMap::iterator iterItem = m_streamHandlers.find( streamSsid );
	-> handlerItem.second->fini(); // mpc::nsdk::ILivingStream::fini
	-> handlerItem.second->release(); // mpc::nsdk::ILivingStream::release
	-> m_rtmpStreamClientMgr->releaseRtmpStreamClient(handlerItem.first); // 停止rtmp拉流
		-> rtmpClient->getRtmpStreamHandler(rtmpStreamHandler);
		-> rtmpStreamHandler->fini(); // 设置m_fmtCtxIn->pb->eof_reached = 1;停止拉流
			-> m_streamData.reset( true ); // 停止缓冲区
			-> m_thread->join(); // 停止ffmpeg处理线程
			-> m_thread.reset();
			-> m_streamData.fini(); // 缓冲区清理
		-> RtmpStreamHandlerFactory::releaseRtmpStreamHandler(rtmpStreamHandler);
		-> delete_o(rtmpClient);
```



# UMG服务