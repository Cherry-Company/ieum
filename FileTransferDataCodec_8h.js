var FileTransferDataCodec_8h =
[
    [ "deskflow::filetransfer::FileTransferDataRoute", "structdeskflow_1_1filetransfer_1_1FileTransferDataRoute.html", "structdeskflow_1_1filetransfer_1_1FileTransferDataRoute" ],
    [ "deskflow::filetransfer::FileTransferDataBegin", "structdeskflow_1_1filetransfer_1_1FileTransferDataBegin.html", "structdeskflow_1_1filetransfer_1_1FileTransferDataBegin" ],
    [ "deskflow::filetransfer::FileTransferDataChunk", "structdeskflow_1_1filetransfer_1_1FileTransferDataChunk.html", "structdeskflow_1_1filetransfer_1_1FileTransferDataChunk" ],
    [ "deskflow::filetransfer::FileTransferDataItemEnd", "structdeskflow_1_1filetransfer_1_1FileTransferDataItemEnd.html", "structdeskflow_1_1filetransfer_1_1FileTransferDataItemEnd" ],
    [ "deskflow::filetransfer::FileTransferDataFinish", "structdeskflow_1_1filetransfer_1_1FileTransferDataFinish.html", "structdeskflow_1_1filetransfer_1_1FileTransferDataFinish" ],
    [ "deskflow::filetransfer::FileTransferDataLimits", "structdeskflow_1_1filetransfer_1_1FileTransferDataLimits.html", "structdeskflow_1_1filetransfer_1_1FileTransferDataLimits" ],
    [ "deskflow::filetransfer::FileTransferDataEncodeResult", "structdeskflow_1_1filetransfer_1_1FileTransferDataEncodeResult.html", "structdeskflow_1_1filetransfer_1_1FileTransferDataEncodeResult" ],
    [ "deskflow::filetransfer::FileTransferDataDecodeResult", "structdeskflow_1_1filetransfer_1_1FileTransferDataDecodeResult.html", "structdeskflow_1_1filetransfer_1_1FileTransferDataDecodeResult" ],
    [ "deskflow::filetransfer::FileTransferDataMessage", "namespacedeskflow_1_1filetransfer.html#a08f2c6ff5c48f120d01a4b0812e60476", null ],
    [ "deskflow::filetransfer::FileTransferDigest", "namespacedeskflow_1_1filetransfer.html#a9309ad76fd6eb18aabefec0b8d81c29b", null ],
    [ "deskflow::filetransfer::FileTransferDataCodecError", "namespacedeskflow_1_1filetransfer.html#af031acb59d2f774f1e3be75d1dd2430d", [
      [ "deskflow::filetransfer::FileTransferDataCodecError::None", "namespacedeskflow_1_1filetransfer.html#af031acb59d2f774f1e3be75d1dd2430da6adf97f83acf6453d4a6a4b1070f3754", null ],
      [ "deskflow::filetransfer::FileTransferDataCodecError::MessageTooLarge", "namespacedeskflow_1_1filetransfer.html#af031acb59d2f774f1e3be75d1dd2430da2cd948594f7c394f0c12f71c5491e9fd", null ],
      [ "deskflow::filetransfer::FileTransferDataCodecError::InvalidMagic", "namespacedeskflow_1_1filetransfer.html#af031acb59d2f774f1e3be75d1dd2430da180818c6d53193ec0e26551c5eab121d", null ],
      [ "deskflow::filetransfer::FileTransferDataCodecError::UnsupportedVersion", "namespacedeskflow_1_1filetransfer.html#af031acb59d2f774f1e3be75d1dd2430da0f89bc98e9b12bdeda0604e57bdc0518", null ],
      [ "deskflow::filetransfer::FileTransferDataCodecError::InvalidKind", "namespacedeskflow_1_1filetransfer.html#af031acb59d2f774f1e3be75d1dd2430da1aa8bfe6c8b095aede66da8191fdaafd", null ],
      [ "deskflow::filetransfer::FileTransferDataCodecError::InvalidFlags", "namespacedeskflow_1_1filetransfer.html#af031acb59d2f774f1e3be75d1dd2430da4abead48d5c11c516f5e7cd78e204dcc", null ],
      [ "deskflow::filetransfer::FileTransferDataCodecError::LengthMismatch", "namespacedeskflow_1_1filetransfer.html#af031acb59d2f774f1e3be75d1dd2430daefe3ddb6bb6a51712447485fd452d87d", null ],
      [ "deskflow::filetransfer::FileTransferDataCodecError::Truncated", "namespacedeskflow_1_1filetransfer.html#af031acb59d2f774f1e3be75d1dd2430da442d803e8762d806fafbf15618e53500", null ],
      [ "deskflow::filetransfer::FileTransferDataCodecError::TrailingData", "namespacedeskflow_1_1filetransfer.html#af031acb59d2f774f1e3be75d1dd2430daedadb80ff5cc940c2d5eb3190ca47e60", null ],
      [ "deskflow::filetransfer::FileTransferDataCodecError::StringTooLong", "namespacedeskflow_1_1filetransfer.html#af031acb59d2f774f1e3be75d1dd2430da74762b647a1c9394ca3b8aa84a2005af", null ],
      [ "deskflow::filetransfer::FileTransferDataCodecError::ValueTooLarge", "namespacedeskflow_1_1filetransfer.html#af031acb59d2f774f1e3be75d1dd2430dafef29e52523c3585a40d3631aa4f1dbe", null ],
      [ "deskflow::filetransfer::FileTransferDataCodecError::EmptyChunk", "namespacedeskflow_1_1filetransfer.html#af031acb59d2f774f1e3be75d1dd2430dae2b281eb4113486c4da14ce1fac17691", null ],
      [ "deskflow::filetransfer::FileTransferDataCodecError::ChunkTooLarge", "namespacedeskflow_1_1filetransfer.html#af031acb59d2f774f1e3be75d1dd2430dac86b18ee3bcaf6bb2cca217e6c576df7", null ],
      [ "deskflow::filetransfer::FileTransferDataCodecError::InvalidValue", "namespacedeskflow_1_1filetransfer.html#af031acb59d2f774f1e3be75d1dd2430da223e81e8afa42c41346a6696560ecc7b", null ],
      [ "deskflow::filetransfer::FileTransferDataCodecError::PolicyRejected", "namespacedeskflow_1_1filetransfer.html#af031acb59d2f774f1e3be75d1dd2430da68bcc065195123f4794abd900a7781b5", null ]
    ] ],
    [ "deskflow::filetransfer::decodeFileTransferData", "namespacedeskflow_1_1filetransfer.html#ac89b77f565c4f41d84f9fb1d301e0fd9", null ],
    [ "deskflow::filetransfer::encodeFileTransferData", "namespacedeskflow_1_1filetransfer.html#ab82269e53a5fe7979b390663929c6b30", null ]
];