#!/bin/bash
# =============================================================================
# install-dmic.sh — MP34DT05TR DMIC para Arduino UNO Q (QRB2210 / Imola)
#
# Conexión física:
#   MP34DT05TR CLK  → JMISC pin 46 (LPI_GPIO6)
#   MP34DT05TR DATA → JMISC pin 48 (LPI_GPIO7)
#   MP34DT05TR VDD  → JMISC 3.3V
#   MP34DT05TR GND  → JMISC GND
#   MP34DT05TR SEL  → GND (canal Left)
#
# Uso:
#   chmod +x install-dmic.sh
#   sudo ./install-dmic.sh
#   sudo reboot
# =============================================================================

set -e

# --- Colores ---
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

log()  { echo -e "${GREEN}[OK]${NC} $1"; }
warn() { echo -e "${YELLOW}[!!]${NC} $1"; }
die()  { echo -e "${RED}[ERR]${NC} $1"; exit 1; }

# --- Verificar root ---
[ "$(id -u)" -eq 0 ] || die "Ejecutar como root: sudo $0"

# --- Verificar hardware ---
log "Verificando hardware..."
compatible=$(cat /sys/firmware/devicetree/base/compatible 2>/dev/null | tr '\0' '\n')
echo "$compatible" | grep -q "qrb2210\|qcm2290" || \
  die "Este script es solo para Arduino UNO Q (QRB2210/QCM2290)"
log "Hardware: QRB2210 / Arduino Imola ✓"

# --- Verificar dependencias ---
for cmd in dtc fdtoverlay fdtput fdtget; do
  command -v "$cmd" >/dev/null 2>&1 || {
    warn "Instalando device-tree-compiler..."
    apt-get install -y device-tree-compiler -q
    break
  }
done

# --- Paths ---
DTB_ORIG="/boot/efi/dtb/qcom/qrb2210-arduino-imola.dtb"
DTB_DMIC="/boot/efi/dtb/qcom/qrb2210-arduino-imola-dmic.dtb"
DTB_BKUP="/boot/efi/dtb/qcom/qrb2210-arduino-imola.dtb.bak"
CONF=$(ls /boot/efi/loader/entries/*.conf 2>/dev/null | head -1)
WORK=$(mktemp -d)
trap "rm -rf $WORK" EXIT

[ -f "$DTB_ORIG" ] || die "DTB original no encontrado: $DTB_ORIG"
[ -n "$CONF" ]     || die "No se encontró entrada de boot en /boot/efi/loader/entries/"

log "DTB original: $DTB_ORIG"
log "Boot conf:    $CONF"

# --- Backup ---
if [ ! -f "$DTB_BKUP" ]; then
  cp "$DTB_ORIG" "$DTB_BKUP"
  log "Backup creado: $DTB_BKUP"
else
  warn "Backup ya existe: $DTB_BKUP"
fi

# --- Overlay 1: pinctrl + q6apm ---
log "Generando overlay q6apm..."
cat << 'DTSEOF' > "$WORK/overlay-q6apm.dts"
/dts-v1/;
/plugin/;
/ {
  compatible = "arduino,imola", "qcom,qcm2290", "qcom,qrb2210";

  fragment@0 {
    target-path = "/soc@0/pinctrl@a7c0000";
    __overlay__ {
      lpi-dmic01-active-state {
        clk-pins {
          pins = "gpio6";
          function = "dmic01_clk";
          drive-strength = <8>;
          bias-disable;
          output-high;
        };
        data-pins {
          pins = "gpio7";
          function = "dmic01_data";
          drive-strength = <8>;
          bias-pull-down;
        };
      };
    };
  };

  fragment@1 {
    target-path = "/soc@0/remoteproc@ab00000/glink-edge/apr";
    __overlay__ {
      #address-cells = <1>;
      #size-cells = <0>;
      service@1 {
        compatible = "qcom,q6apm";
        reg = <0x01>;
        qcom,protection-domain = "avs/audio", "msm/adsp/audio_pd";
        dais {
          compatible = "qcom,q6apm-lpass-dais";
          #sound-dai-cells = <1>;
        };
      };
    };
  };
};
DTSEOF

dtc -@ -I dts -O dtb -o "$WORK/overlay-q6apm.dtbo" "$WORK/overlay-q6apm.dts" 2>/dev/null
fdtoverlay \
  -i "$DTB_ORIG" \
  -o "$WORK/step1.dtb" \
  "$WORK/overlay-q6apm.dtbo"
log "Overlay q6apm aplicado ✓"

# --- Encontrar el phandle máximo en step1.dtb ---
log "Buscando phandle disponible..."
MAX_PHANDLE=$(fdtdump "$WORK/step1.dtb" 2>/dev/null | \
  grep "phandle = " | \
  sed 's/.*<0x\([0-9a-fA-F]*\)>.*/\1/' | \
  sort -t'x' -k1 | \
  awk 'BEGIN{max=0} {v=strtonum("0x"$1); if(v>max)max=v} END{printf "%d\n",max}')
NEW_PHANDLE=$((MAX_PHANDLE + 1))
NEW_PHANDLE_HEX=$(printf "0x%x" $NEW_PHANDLE)
log "Phandle asignado para q6apm-dais: $NEW_PHANDLE_HEX"

# --- Overlay 2: sound DAI link ---
log "Generando overlay sound link..."
cat << DTSEOF > "$WORK/overlay-sound.dts"
/dts-v1/;
/plugin/;
/ {
  compatible = "arduino,imola", "qcom,qcm2290", "qcom,qrb2210";

  fragment@0 {
    target-path = "/sound";
    __overlay__ {
      dmic-capture-dai-link {
        link-name = "DMIC Capture";
        cpu {
          sound-dai = <$NEW_PHANDLE_HEX 45>;
        };
        codec {
          /* vamacro phandle=0x1e, DMIC0 cell=0 */
          sound-dai = <0x1e 0x00>;
        };
      };
    };
  };
};
DTSEOF

dtc -@ -I dts -O dtb -o "$WORK/overlay-sound.dtbo" "$WORK/overlay-sound.dts" 2>/dev/null
fdtoverlay \
  -i "$WORK/step1.dtb" \
  -o "$WORK/final.dtb" \
  "$WORK/overlay-sound.dtbo"
log "Overlay sound link aplicado ✓"

# --- Asignar phandle al nodo dais en el DTB final ---
log "Asignando phandle al nodo q6apm-dais..."
fdtput -t u "$WORK/final.dtb" \
  "/soc@0/remoteproc@ab00000/glink-edge/apr/service@1/dais" \
  phandle $NEW_PHANDLE

# Verificar
PHANDLE_CHECK=$(fdtget -t u "$WORK/final.dtb" \
  "/soc@0/remoteproc@ab00000/glink-edge/apr/service@1/dais" phandle 2>/dev/null)
[ "$PHANDLE_CHECK" = "$NEW_PHANDLE" ] || \
  die "Fallo al verificar phandle (esperado $NEW_PHANDLE, obtenido $PHANDLE_CHECK)"
log "Phandle verificado: $PHANDLE_CHECK ✓"

# --- Verificar que el sound link tiene el phandle correcto ---
SOUND_PHANDLE=$(fdtdump "$WORK/final.dtb" 2>/dev/null | \
  grep -A10 "DMIC Capture" | grep "cpu" -A2 | grep "sound-dai" | \
  sed 's/.*<0x\([0-9a-fA-F]*\).*/\1/')
log "Sound DAI cpu phandle en DTB: 0x$SOUND_PHANDLE"

# --- Instalar DTB final ---
cp "$WORK/final.dtb" "$DTB_DMIC"
log "DTB instalado: $DTB_DMIC"

# --- Actualizar boot conf ---
# Eliminar entrada devicetree anterior si existe
sed -i '/^devicetree/d' "$CONF"
# Agregar nueva entrada después de initrd
sed -i '/^initrd/a devicetree /dtb/qcom/qrb2210-arduino-imola-dmic.dtb' "$CONF"

# Verificar que quedó bien
grep -q "devicetree /dtb/qcom/qrb2210-arduino-imola-dmic.dtb" "$CONF" || \
  die "Fallo al actualizar boot conf"
log "Boot conf actualizado ✓"

# --- Guardar el DTS para referencia ---
cp "$WORK/overlay-q6apm.dts" "/boot/efi/dtb/qcom/arduino-imola-dmic-q6apm.dts"
cp "$WORK/overlay-sound.dts"  "/boot/efi/dtb/qcom/arduino-imola-dmic-sound.dts"
log "DTS fuentes guardados en /boot/efi/dtb/qcom/"

# --- Resumen ---
echo ""
echo "============================================="
echo -e "${GREEN}  Instalación completa${NC}"
echo "============================================="
echo "  DTB original:  $DTB_BKUP (backup)"
echo "  DTB nuevo:     $DTB_DMIC"
echo "  Boot conf:     $CONF"
echo ""
echo "  Conexión física MP34DT05TR:"
echo "    CLK  → JMISC pin 46"
echo "    DATA → JMISC pin 48"
echo "    VDD  → JMISC 3.3V"
echo "    GND  → JMISC GND"
echo "    SEL  → GND"
echo ""
echo "  Después del reboot verificar con:"
echo "    arecord -l"
echo "    arecord -D hw:0,45 -f S16_LE -r 16000 -c 1 -d 5 test.wav"
echo "============================================="
echo ""
warn "Reinicia el sistema para aplicar los cambios"
echo "  sudo reboot"
