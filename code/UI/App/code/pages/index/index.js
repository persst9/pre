const mqtt = require('../../utils/mqtt.min.js');
// import mqtt from "../../utils/mqtt.min.js";
const mqttUrl = 'qa1ba1e7.ala.cn-hangzhou.emqxsl.cn';
// import mqtt from "../../utils/mqtt.min.js"
let nodeId = "";
let nodeidx = 1;
let nodeIdCount = 1;
Page({
  data: {
    currentNodeIndex: 0,
    lastUpdateTime: '从未更新',
    lastClickTime: 0,
    client: null,
    nodes: [
      { 
        id: 1, // 建议用 ID 作为 MQTT 的 Topic 标识
        name: "1号蔬菜棚", 
        light: 0, temp: 0, hum: 0, soil: 0, fan: false, pump: false,LED:false,
        threshold: { temp: 30, soil: 20, light: 15000, hum: 80 } 
      }
    ],
    // MQTT 配置
    host: mqttUrl,
    mqttOptions: {
      username: "lin",
      password: "123456",
      reconnectPeriod: 1000,
      connectTimeout: 30 * 1000,
      protocol: 'wxs', // 必须显式指定协议，解决 s is not a constructor 报错
    },
  },

  onLoad() {
    this.connectMqtt();
  },
// 找到最小可用节点ID
getNextNodeId(nodes) {

  const usedIds = nodes.map(n => parseInt(n.id));

  let id = 1;

  while (usedIds.includes(id)) {
    id++;
  }

  return String(id);
},
 // 连接 MQTT
 connectMqtt() {
  const clientId = 'wx_farm_' + new Date().getTime();
  try {
    // 官方建议使用 8084 端口配合 wxs
    this.data.client = mqtt.connect(`wxs://${this.data.host}:8084/mqtt`, {
      ...this.data.mqttOptions,
      clientId,
    });

    this.data.client.on("connect", () => {
      console.log("MQTT 已连接");
      this.setData({ 'client.connected': true });
      
      // 自动订阅所有节点的 Topic
      this.data.nodes.forEach(node => {
        const topic = `/smart/sub`;
        this.data.client.subscribe(topic);
        console.log("已订阅:", topic);
      });
    });

    // 收到消息处理
    this.data.client.on("message", (topic, payload) => {
      try {
        const json = JSON.parse(payload.toString());
        this.handleMqttMessage(topic, json);
      } catch (e) {
        console.error("解析消息失败", e);
      }
    });

    this.data.client.on("error", (err) => console.log("MQTT Error:", err));
    this.data.client.on("offline", () => this.setData({ 'client.connected': false }));

  } catch (error) {
    console.log("mqtt.connect error", error);
  }
},

// 处理收到的传感器数据
handleMqttMessage(topic, data) {
  const nodes = this.data.nodes;
  
  // 修改这里：优先通过数据包里的 id 来查找大棚
  let index = nodes.findIndex(n => n.id === data.id);

  // 如果数据包没带 id，再尝试从主题路径里找（保留兼容性）
  if (index === -1) {
    index = nodes.findIndex(n => topic.includes(n.id));
  }

  if (index !== -1) {
    console.log("匹配成功，正在更新节点：", nodes[index].name);
    const now = new Date();
    const timeStr = `${now.getHours()}:${now.getMinutes().toString().padStart(2,'0')}:${now.getSeconds().toString().padStart(2,'0')}`;
    
    // 使用动态键名更新数据，性能更好且不会覆盖整个对象
    const base = `nodes[${index}]`;
    this.setData({
      [`${base}.temp`]: data.temp ?? nodes[index].temp,
      [`${base}.hum`]: data.humi ?? nodes[index].hum,
      [`${base}.soil`]: data.soil ?? nodes[index].soil,
      [`${base}.light`]: data.light ?? nodes[index].light,
      [`${base}.fan`]: data.fan === 1,
      [`${base}.pump`]: data.pump === 1,
      [`${base}.LED`]: data.LED === 1,
      lastUpdateTime: timeStr
    });
  } else {
    console.error("【匹配失败】收到的消息 ID 为:", data.id, "，但本地节点列表只有:", nodes.map(n => n.id));
  }
},

// 控制开关并发布消息
// 在 Page 对象中添加/替换此函数
toggleSwitch(e) {

  const type = e.currentTarget.dataset.type; // fan / pump
  const val = e.detail.value;

  const node = this.data.nodes[this.data.currentNodeIndex];

  // 当前状态
  let fan = node.fan ? 1 : 0;
  let pump = node.pump ? 1 : 0;
  let led  = node.LED  ? 1 : 0;
  // 更新当前操作的设备
  if (type === 'fan') {
    fan = val ? 1 : 0;
  } else if (type === 'pump') {
    pump = val ? 1 : 0;
  }else if (type === 'LED') {
    led = val ? 1 : 0;
  }

  if (this.data.client && this.data.client.connected) {

    const ctrlTopic = "smart/pub";

    const payload = JSON.stringify({
      action: "Control",
      id: node.id,
      f: fan,
      p: pump,
      L: led
    });

    this.data.client.publish(ctrlTopic, payload, { qos: 0 }, (err) => {

      if (err) {
        console.error("发布失败:", err);
      } else {

        console.log("发布成功:", ctrlTopic, payload);

        // 更新UI
        const base = `nodes[${this.data.currentNodeIndex}]`;

        this.setData({
          [`${base}.fan`]: fan === 1,
          [`${base}.pump`]: pump === 1,
          [`${base}.LED`]: led === 1
        });

      }

    });

  } else {

    wx.showToast({
      title: '服务器未连接',
      icon: 'none'
    });

    console.error("MQTT 未连接");

  }

},
// 双击改名逻辑
handleNameTap(e) {
  const nowTime = e.timeStamp;
  const lastTime = this.data.lastClickTime;
  
  if (nowTime - lastTime < 300) {
    // 触发双击
    this.renameNode();
  }
  this.setData({ lastClickTime: nowTime });
},

// 改名功能
renameNode() {
  const index = this.data.currentNodeIndex;
  wx.showModal({
    title: '修改名称',
    editable: true,
    content: this.data.nodes[index].name,
    success: (res) => {
      if (res.confirm && res.content) {
        let nodes = this.data.nodes;
        nodes[index].name = res.content;
        this.setData({ nodes });
      }
    }
  });
},

// 统一的阈值设置函数
setThreshold() {

  const index = this.data.currentNodeIndex;
  const node = this.data.nodes[index];

  const items = ['温度上限', '土壤湿度下限', '光照上限', '环境湿度上限'];
  const keys = ['temp', 'soil', 'light', 'hum'];

  wx.showActionSheet({
    itemList: items,

    success: (res) => {

      const selectedKey = keys[res.tapIndex];
      const selectedName = items[res.tapIndex];

      wx.showModal({
        title: `修改${selectedName}`,
        content: `${node.threshold[selectedKey]}`,
        editable: true,
        placeholderText: '请输入新的阈值',

        success: (confirmRes) => {

          if (confirmRes.confirm && confirmRes.content) {

            const newVal = parseFloat(confirmRes.content);

            if (isNaN(newVal)) {
              wx.showToast({
                title: '请输入数字',
                icon: 'none'
              });
              return;
            }

            let nodes = [...this.data.nodes];

            nodes[index].threshold[selectedKey] = newVal;

            this.setData({ nodes });

            wx.showToast({
              title: '设置成功'
            });

            // ======================
            // MQTT 发送阈值
            // ======================

            if (this.data.client && this.data.client.connected) {

              const payload = JSON.stringify({
                type: "threshold",
                id: node.id,
                key: selectedKey,
                value: newVal
              });

              this.data.client.publish("smart/pub", payload);

              console.log("阈值发送:", payload);

            } else {

              console.log("MQTT未连接，阈值未发送");

            }

          }

        }

      });

    }

  });

},
  // 切换节点
  switchNode(e) {
    this.setData({ currentNodeIndex: e.currentTarget.dataset.index });
  },

  // 添加节点
  addNode() {

    wx.showModal({
      title: '添加新大棚',
      placeholderText: '请输入大棚名称',
      editable: true,
  
      success: (res) => {
  
        if (res.confirm && res.content) {
  
          const newName = res.content.trim();
  
          if (newName === '') {
            wx.showToast({
              title: '名称不能为空',
              icon: 'none'
            });
            return;
          }
  
          let newNodes = this.data.nodes;
  
          const newId = this.getNextNodeId(newNodes);
  
          const newNode = {
            id: newId,
            name: newName,
            light: 0,
            temp: 0,
            hum: 0,
            soil: 0,
            fan: false,
            pump: false,
            threshold: { temp: 30, soil: 20, light: 15000, hum: 80 }
          };
  
          newNodes.push(newNode);
  
          this.setData({
            nodes: newNodes,
            currentNodeIndex: newNodes.length - 1
          });
  
          wx.showToast({
            title: '添加完成'
          });
  
          console.log("新节点ID:", newId);
  
          // MQTT通知服务器
          if (this.data.client && this.data.client.connected) {
  
            const payload = JSON.stringify({
              action: "addNode",
              node: newNode
            });
  
            this.data.client.publish("smart/pub", payload);
          }
  
        }
      }
    });
  },
  deleteNode(e) {

    const index = e.currentTarget.dataset.index;
    const nodeName = this.data.nodes[index].name;
  
    wx.showModal({
      title: '删除确认',
      content: `确定要删除“${nodeName}”吗？`,
      confirmColor: '#ff5252',
  
      success: (res) => {
  
        if (res.confirm) {
  
          let newNodes = this.data.nodes;
  
          if (newNodes.length <= 1) {
            wx.showToast({
              title: '至少保留一个大棚',
              icon: 'none'
            });
            return;
          }
  
          const removedNode = newNodes[index];
  
          newNodes.splice(index, 1);
  
          // // 重新整理 ID
          // newNodes = this.resetNodeIds(newNodes);
  
          let newIndex = this.data.currentNodeIndex;
  
          // if (newIndex >= newNodes.length) {
          //   newIndex = newNodes.length - 1;
          // }
  
          this.setData({
            nodes: newNodes,
            currentNodeIndex: newIndex
          });
  
          // MQTT 通知服务器删除节点
          if (this.data.client && this.data.client.connected) {
  
            const payload = JSON.stringify({
              action: "delete",
              nodeId: removedNode.id
            });
  
            this.data.client.publish("smart/pub", payload);
          }
  
          wx.showToast({
            title: '已删除',
            icon: 'success'
          });
        }
      }
    });
  },

  // 开关控制逻辑
  toggleFan(e) {
    let val = e.detail.value;
    console.log("风扇状态：", val);
    // 这里后续对接云函数或MQTT指令
  }
})