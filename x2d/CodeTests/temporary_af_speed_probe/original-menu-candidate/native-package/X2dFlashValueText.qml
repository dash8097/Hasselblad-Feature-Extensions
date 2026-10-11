import QtQuick 2.5

X2dFlashText {
    id: valueText
    // 数字/大写字的行框包含下伸留白；光学下移不改变触摸区域和行高。
    // OFF 和加减符号保持原位置，字号较大的功率详情使用相应偏移。
    transform: Translate {
        y: valueText.text==="OFF" || valueText.text==="+" || valueText.text==="−" || valueText.text==="—"
           ? 0 : (valueText.font.pixelSize>=60 ? 5 : 2)
    }
}
