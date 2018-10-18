using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;

namespace RAD_PAY.BusinessLogic.ViewModels
{
    public class top_up_transactionsViewModel
    {
        public long id { get; set; }
        public int? topup_id { get; set; }
        public long? amount_sum { get; set; }
        public double? amount_req { get; set; }
        public int? currency { get; set; }
        public long? uid { get; set; }
        public string login { get; set; }
        public DateTime? ts { get; set; }
        public DateTime? tse { get; set; }
        public int? status { get; set; }
        public string status_text { get; set; }
        public long? card_id { get; set; }
        public string card_pan { get; set; }
        public string eopc_trn_id { get; set; }
        public long? rad_card { get; set; }
        public string pay_description { get; set; }
        public long? topup_trn_id { get; set; }
    }
}