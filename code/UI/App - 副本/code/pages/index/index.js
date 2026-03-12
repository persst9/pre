
Page({
  data: {
    currentNodeIndex: 0,
    lastClickTime: 0,
    nodes: [
      { 
        id: 1, name: "1号蔬菜棚", 
        light: 12000, temp: 32, hum: 55, soil: 40, fan: false, pump: true,
        // 全属性阈值
        threshold: { 
          temp: 30,    // 温度上限
          soil: 20,    // 土壤湿度下限
          light: 15000,// 光照上限
          hum: 80      // 环境湿度上限
        } 
      }
    ]
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
  const items = ['温度上限', '土壤湿度下限', '光照上限', '环境湿度上限'];
  const keys = ['temp', 'soil', 'light', 'hum'];

  wx.showActionSheet({
    itemList: items,
    success: (res) => {
      const selectedKey = keys[res.tapIndex];
      const selectedName = items[res.tapIndex];
      
      wx.showModal({
        title: `修改${selectedName}`,
        content: `${this.data.nodes[index].threshold[selectedKey]}`,
        editable: true,
        placeholderText: '请输入新的阈值',
        success: (confirmRes) => {
          if (confirmRes.confirm && confirmRes.content) {
            const newVal = parseFloat(confirmRes.content);
            if (isNaN(newVal)) {
              wx.showToast({ title: '请输入数字', icon: 'none' });
              return;
            }
            
            let nodes = this.data.nodes;
            nodes[index].threshold[selectedKey] = newVal;
            this.setData({ nodes });
            wx.showToast({ title: '设置成功' });
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
      editable: true, // 启用输入框
      success: (res) => {
        if (res.confirm && res.content) {
          const newName = res.content.trim();
          if (newName === '') {
            wx.showToast({ title: '名称不能为空', icon: 'none' });
            return;
          }

          let newNodes = this.data.nodes;
          const newId = Date.now(); // 使用时间戳生成临时唯一ID
          
          // 创建新节点对象，初始值设为0
          const newNode = {
            id: newId,
            name: newName,
            light: 0,
            temp: 0,
            hum: 0,
            soil: 0,
            fan: false,
            pump: false
          };

          newNodes.push(newNode);
          
          this.setData({
            nodes: newNodes,
            currentNodeIndex: newNodes.length - 1 // 添加后自动跳转到新节点
          });

          wx.showToast({ title: '添加完成' });
        } else if (res.confirm && !res.content) {
          wx.showToast({ title: '请输入名称', icon: 'none' });
        }
      }
    });
  },

  deleteNode(e) {
    const index = e.currentTarget.dataset.index;
    const nodeName = this.data.nodes[index].name;

    // 弹出确认提示框
    wx.showModal({
      title: '删除确认',
      content: `确定要删除“${nodeName}”吗？删除后数据不可恢复。`,
      confirmColor: '#ff5252', // 将确认按钮设为红色，起到警示作用
      cancelText: '取消',
      confirmText: '删除',
      success: (res) => {
        if (res.confirm) {
          // 用户点击了确定
          let newNodes = this.data.nodes;

          // 业务逻辑：至少保留一个大棚
          if (newNodes.length <= 1) {
            wx.showToast({
              title: '请至少保留一个大棚',
              icon: 'none'
            });
            return;
          }

          // 执行删除
          newNodes.splice(index, 1);

          // 更新索引，防止数组越界
          let newIndex = this.data.currentNodeIndex;
          if (newIndex >= newNodes.length) {
            newIndex = newNodes.length - 1;
          }

          this.setData({
            nodes: newNodes,
            currentNodeIndex: newIndex
          });

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