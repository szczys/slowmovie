package api

import (
	"context"
	"encoding/json"
)

type slowmovieCTX struct {
	frameNum  int
	frameBin  string
	fwVer     string
	fwPackage string
}

var CTX = slowmovieCTX{
	frameNum: 0,
	frameBin:  "",
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
