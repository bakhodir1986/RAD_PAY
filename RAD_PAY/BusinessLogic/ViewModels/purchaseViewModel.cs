using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;

namespace RAD_PAY.BusinessLogic.ViewModels
{
    public class purchaseViewModel
    {
        public long id { get; set; }
        public int merchant_id { get; set; }
        public string login { get; set; }
        public long? amount { get; set; }
        public DateTime? ts { get; set; }
        public long? uid { get; set; }
        public string transaction_id { get; set; }
        public string pan { get; set; }
        public int? status { get; set; }
        public string paynet_tr_id { get; set; }
        public int paynet_status { get; set; }
        public long? receipt { get; set; }
        public long rad_paynet_tr_id { get; set; }
        public long? rad_tr_id { get; set; }
        public long? card_id { get; set; }
        public long? bearn { get; set; }
        public long commission { get; set; }
        public string merch_rsp { get; set; }
    }
}