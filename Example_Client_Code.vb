Imports System.Net.Sockets
Imports System.Text
Imports System.Net.Dns
Imports System.Net

Public Class Chat_Main
    Dim clientSocket As New System.Net.Sockets.TcpClient()
    Dim serverstream As NetworkStream
    Dim readData As String
    Dim infiniteCounter As Integer
    Dim serverbytes(10024) As Byte
    Public username As String
    Public password As String
    Public server As String
    Public room As String
    Public proceed As Boolean = True

    Sub main()

        Dim ipAddress As IPAddress = Dns.Resolve(server).AddressList(0)
        clientSocket.Connect(ipAddress, 4545)
        serverstream = clientSocket.GetStream()

        Dim tempmsg As String = username + "~" + password + "~" + room + "~"
        Dim outstream As Byte() = System.Text.Encoding.ASCII.GetBytes(tempmsg)
        serverstream.Write(outstream, 0, outstream.Length)
        serverstream.Flush()

        serverstream.Read(serverbytes, 0, CInt(clientSocket.ReceiveBufferSize))
        readData = System.Text.Encoding.ASCII.GetString(serverbytes)
        readData = readData.Substring(0, readData.IndexOf("~"))

        If (readData.Equals("valid")) Then
            Me.Enabled = True
            Chat_Login.Hide()
            Me.Show()

            lblUser.Text = Chat_Login.txtUsername.Text + " :"
            Me.Text = "TAMUCC Chat - " + room
            Dim ctThread As Threading.Thread = New Threading.Thread(AddressOf getMessage)
            ctThread.IsBackground = True
            ctThread.Start()

        Else
            MsgBox("invalid")
            Chat_Login.txtUsername.Clear()
            Chat_Login.txtPassword.Clear()

        End If


    End Sub

    Private Sub txtSend_KeyPress(ByVal sender As System.Object, ByVal e As System.Windows.Forms.KeyPressEventArgs) Handles txtSend.KeyPress
        If (e.KeyChar = Chr(13)) Then
            If (String.IsNullOrEmpty(txtSend.Text)) Then

            Else
                Dim outstream As Byte() = System.Text.Encoding.ASCII.GetBytes(txtSend.Text + "~")
                serverstream.Write(outstream, 0, outstream.Length)
                serverstream.Flush()
                txtSend.Clear()
            End If
        End If
    End Sub

    Private Sub msg()
        If (proceed = True) Then
            If Me.InvokeRequired Then
                Me.Invoke(New MethodInvoker(AddressOf msg))
            Else
                txtAll.Text = txtAll.Text + Environment.NewLine + readData
                txtAll.Select(txtAll.Text.Length, 0)
                txtAll.ScrollToCaret()
            End If
        End If
    End Sub

    Private Sub getMessage()
        For infiniteCounter = 1 To 2
            infiniteCounter = 1
            serverstream = clientSocket.GetStream()
            Dim inStream(10024) As Byte
            serverstream.Read(inStream, 0, CInt(clientSocket.ReceiveBufferSize))
            Dim returndata As String = System.Text.Encoding.ASCII.GetString(inStream)

            readData = "" + returndata

            If (readData.Contains("$kicked") Or returndata.Contains("$kicked")) Then

                MsgBox("You have been kicked", MsgBoxStyle.Critical, "--" + username + "--")
                Dim outstream As Byte() = System.Text.Encoding.ASCII.GetBytes("exitchatprogram~")
                serverstream.Write(outstream, 0, outstream.Length)
                serverstream.Flush()
                proceed = False
                Threading.Thread.CurrentThread.Abort()

            ElseIf (String.IsNullOrEmpty(readData)) Then

            Else
                msg()
            End If
        Next
    End Sub
    Private Sub Chat_Main_Load(sender As Object, e As System.EventArgs) Handles Me.Load

        Chat_Login.ShowDialog()
    End Sub

    Private Sub btnExit_Click(sender As System.Object, e As System.EventArgs) Handles btnExit.Click
        Chat_Login.Dispose()
        Me.Dispose()
    End Sub

    Private Sub btnDisconnect_Click(sender As System.Object, e As System.EventArgs) Handles btnDisconnect.Click
        Dim outstream As Byte() = System.Text.Encoding.ASCII.GetBytes("exitchatprogram~")
        serverstream.Write(outstream, 0, outstream.Length)
        serverstream.Flush()
        txtSend.Enabled = False
        btnDisconnect.Enabled = False
        proceed = False
    End Sub
End Class