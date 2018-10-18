using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;

namespace RAD_PAY.BusinessLogic.ViewModels
{
    public class currencyViewModel
    {
        public long id { get; set; }
        public double? usd_uzs { get; set; }
        public double? usd_rub { get; set; }
        public double? usd_eur { get; set; }
        public DateTime? upd_ts { get; set; }
        public DateTime? expiry_ts { get; set; }
        public int type { get; set; }
    }
}