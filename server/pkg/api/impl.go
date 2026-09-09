package api

import (
	"context"
	"encoding/json"
	"fmt"
	"path/filepath"
	"os"
)

type slowmovieCTX struct {
	frameNum  int
	framePath string
	fwVer     string
	fwPackage string
}

var CTX = slowmovieCTX{
	frameNum: 0,
	framePath: filepath.Join(os.TempDir(), "frame.zz"),
	fwVer:     "",
	fwPackage: "",
}

type Server struct{}

func ConvertToMap(v any) (map[string]any, error) {
	jsonData, err := json.Marshal(v)
	if err != nil {
		return nil, err
	}

	var mapData map[string]any
	err = json.Unmarshal(jsonData, &mapData)
	return mapData, err
}

func NewServer() Server {
	return Server{}
}

func (Server) GetStatus(ctx context.Context, request GetStatusRequestObject) (GetStatusResponseObject, error) {
	statusData, err := ConvertToMap(CTX)
	return GetStatus200JSONResponse{Data: statusData}, err
}

func (Server) PublishNewFrame(ctx context.Context, request PublishNewFrameRequestObject) (PublishNewFrameResponseObject, error) {
	f, err := os.Create(CTX.framePath)
	if err != nil {
		return PublishNewFrame500JSONResponse{}, nil
	}

	defer f.Close()
	_, err = f.ReadFrom(request.Body)
	if err != nil {
		return PublishNewFrame500JSONResponse{}, nil
	}

	CTX.frameNum += 1
	return PublishNewFrame201JSONResponse{
		Data: map[string]any { "frameNum": CTX.frameNum },
	}, err
}


func (Server) GetFrameByVersion(ctx context.Context, request GetFrameByVersionRequestObject) (GetFrameByVersionResponseObject, error) {
	if request.Version != fmt.Sprintf("%d", CTX.frameNum) {
		return GetFrameByVersion404JSONResponse{
			Message: fmt.Sprintf("Version %s not found", request.Version),
		}, nil
	}

	f, err := os.Open(CTX.framePath)
	if err != nil {
		return GetFrameByVersion404JSONResponse{
			Message: "file not found",
		}, nil
	}

	fi, err := f.Stat()
	if err != nil {
		return GetFrameByVersion404JSONResponse{
			Message: "Unable to get file size",
		}, nil
	}

	return GetFrameByVersion200ApplicationoctetStreamResponse{
		ContentLength: fi.Size(),
		Body: f,
	}, err
}
