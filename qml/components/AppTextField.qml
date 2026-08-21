import Qcm.Material as MD

// QmlMaterial outlined defaults to title_large + 64dp.
MD.TextField {
    type: MD.Enum.TextFieldOutlined
    typescale: MD.Token.typescale.body_large
    mdState.dense: true
    topPadding: 10
    bottomPadding: 10
}
