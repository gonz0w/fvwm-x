package pinboard

import (
	"strings"
	"testing"
)

func TestSceneKeepsTargetBytesOutOfCommandsAndFitsTransport(t *testing.T) {
	p := Protocol{"FvwmPinboard-" + strings.Repeat("a", 32), strings.Repeat("b", 32)}
	s := Empty()
	pin := testPin()
	pin.Label = strings.Repeat("x", 256)
	pin.Monitor = []byte(strings.Repeat("m", 256))
	s.Pins = []Pin{pin}
	commands, err := p.Scene(0, 1, s, nil, map[string]string{pin.ID: strings.Repeat("p", 384)})
	if err != nil || len(commands) != 6 || !strings.Contains(commands[1], " Layout ") {
		t.Fatal(commands, err)
	}
	for _, command := range commands {
		if len(command) > 1000 || strings.Contains(command, "literal") || strings.Contains(command, "target_bytes") {
			t.Fatal(command)
		}
	}
	if _, err := p.Scene(1, 1, s, nil, nil); err == nil {
		t.Fatal("stale scene accepted")
	}
}

func TestFrozenNativeReplyFormsAndStaleAdmission(t *testing.T) {
	token := strings.Repeat("a", 32)
	id := strings.Repeat("b", 32)
	epoch := "0000000000000001"
	status, err := ParseStatus("FVWM_PINBOARD_STATUS "+token+" t1 1 0 ready 123 "+epoch, token)
	if err != nil || status.Current != 1 || status.Epoch != epoch {
		t.Fatal(status, err)
	}
	event, err := ParseEvent("FVWM_PINBOARD_EVENT "+token+" 1 "+epoch+" move "+id+" 6d 2 3", token)
	if err != nil || event.Column != 2 || string(event.Monitor) != "m" {
		t.Fatal(event, err)
	}
	text := "FVWM_PINBOARD_CELL " + token + " t1 1 " + epoch + " 0 " + id + " 6d 6 2 3 -100 80 144 144 1"
	cell, err := ParseCell(text, token, "t1", 1, epoch)
	if err != nil || cell.X != -100 || cell.Quarters != 6 || !cell.Visible {
		t.Fatal(cell, err)
	}
	if _, err := ParseCell(text, token, "t1", 2, epoch); err == nil {
		t.Fatal("stale cell accepted")
	}
	page, err := ParseCellsEnd("FVWM_PINBOARD_CELLS_END "+token+" t1 1 "+epoch+" 0 1 1", token, "t1", 1, epoch)
	if err != nil || page.Count != 1 {
		t.Fatal(page, err)
	}
	s := Empty()
	p := testPin()
	p.ID = id
	s.Pins = []Pin{p}
	page.Cells = []Cell{cell}
	if err := page.Validate(s, 0); err != nil {
		t.Fatal(err)
	}
	page.Cells[0].Index = 1
	if err := page.Validate(s, 0); err == nil {
		t.Fatal("misordered cell admitted")
	}
	page.Cells = nil
	if err := page.Validate(s, 0); err == nil {
		t.Fatal("partial cell page admitted")
	}
	for _, value := range []string{"01", "-1", "+1", "2147483648"} {
		if _, err := Decimal(value, MaxGeneration); err == nil {
			t.Fatal(value)
		}
	}
}
