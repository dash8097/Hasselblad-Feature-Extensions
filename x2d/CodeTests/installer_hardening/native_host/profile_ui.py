"""仅在双档载荷已绑定时转换旧客户端模板；不修改冻结模板。"""
def replace(text, before, after):
    assert text.count(before) == 1, '双档界面模板变化：'+before[:80]
    return text.replace(before, after)

def windows(text):
    text=replace(text,'readonly CheckBox[] featureBoxes = new CheckBox[7];','readonly RadioButton[] featureBoxes = new RadioButton[2];')
    text=replace(text,'int operationMask = 255;','int operationMask = 248;')
    text=replace(text,'全功能测试包：以下功能整套安装。','基础默认安装；高级包含基础及扩展拍摄。')
    start=text.index('        string[] featureLabels =')
    end=text.index('        var card =',start)
    text=text[:start]+'''        string[] featureLabels = {"基础", "高级（包含基础）"};
        int[] featureBits = {248,255};
        for(int i=0;i<featureBoxes.Length;i++) {
            var box = new RadioButton {Text=featureLabels[i],Name="Feature"+i,Tag=featureBits[i],Checked=i==0,Enabled=true,AutoCheck=true,AutoSize=false,
                Location=new Point(28,154+i*40),Size=new Size(640,30),UseVisualStyleBackColor=true};
            featureBoxes[i]=box; Controls.Add(box);
        }
        Controls.Add(new Label {Text="高级增加像素超频、25MP、RAW／ISO 堆栈。",AutoSize=true,
            Location=new Point(28,250),Font=new Font(Font.FontFamily,9),ForeColor=Color.FromArgb(109,116,126)});
'''+text[end:]
    text=replace(text,'action=action,feature_mask=255,pending=pending','action=action,feature_mask=operationMask,pending=pending')
    text=replace(text,'int SelectedFeatures() {return 255;}','int SelectedFeatures() {return featureBoxes[1].Checked ? 255 : 248;}')
    text=replace(text,'finally { foreach(var box in featureBoxes) box.Enabled=false;','finally { foreach(var box in featureBoxes) box.Enabled=!update.Pending;')
    text=replace(text,'            if(args.Length == 2 && args[0] == "--progress-selftest")', '''            if(args.Length == 2 && args[0] == "--profile-selftest") {
                if(form.featureBoxes.Length!=2 || form.SelectedFeatures()!=248 || !form.featureBoxes[0].Checked) throw new InvalidOperationException("基础默认值错误");
                form.featureBoxes[1].Checked=true;
                if(form.SelectedFeatures()!=255 || form.featureBoxes[0].Checked) throw new InvalidOperationException("高级互斥失败");
                form.featureBoxes[0].Checked=true;
                if(form.SelectedFeatures()!=248 || form.featureBoxes[1].Checked) throw new InvalidOperationException("基础互斥失败");
                File.WriteAllText(args[1],"PROFILE_UI_PASSED device_requests=0 checks=3"); return;
            }
            if(args.Length == 2 && args[0] == "--progress-selftest")''')
    return text

def mac(text):
    text=replace(text,"self.add_method(delegate_class, 'pressed:', self.pressed, None, b'v@:@')","self.add_method(delegate_class, 'pressed:', self.pressed, None, b'v@:@')\n        self.add_method(delegate_class, 'profile:', self.profile_pressed, None, b'v@:@')")
    text=replace(text,'完整功能测试包；下列功能统一安装。','基础默认安装；高级包含基础及扩展拍摄。')
    text=replace(text,"self.send(box,'setButtonType:',None,(C.c_ulong,3))","self.send(box,'setButtonType:',None,(C.c_ulong,4))")
    text=replace(text,"            self.send(box,'setEnabled:',None,(C.c_bool,False))","            self.send(box,'setEnabled:',None,(C.c_bool,True))\n            self.send(box,'setTarget:',None,(C.c_void_p,self.delegate))\n            self.send(box,'setAction:',None,(C.c_void_p,self.selector('profile:')))")
    text=replace(text,'四亿、十六亿、RAW 堆栈及 AFC 开放；不包含 AFC 记录工具。','高级增加像素超频、25MP、RAW／ISO 堆栈。')
    text=replace(text,'    def pressed(self, sender):','''    def profile_pressed(self, sender):
        if self.busy or self.installer.pending:
            return
        for box,bit in self.feature_indicators:
            self.send(box,'setState:',None,(C.c_long,int(box==sender)))

    def pressed(self, sender):''')
    text=replace(text,'        self.installer.feature_mask=255',"        selected=[bit for box,bit in self.feature_indicators if self.send(box,'state',C.c_long)==1]\n        if len(selected)!=1 or selected[0] not in (248,255):\n            raise RuntimeError('请选择基础或高级')\n        self.installer.feature_mask=selected[0]")
    text=replace(text,"for box,bit in getattr(self,'feature_indicators',[]):self.send(box,'setEnabled:',None,(C.c_bool,False))","for box,bit in getattr(self,'feature_indicators',[]):self.send(box,'setEnabled:',None,(C.c_bool,not self.installer.pending))")
    return text
